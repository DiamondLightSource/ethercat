#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "msgq.h"

#define CLOCK_TO_USE CLOCK_MONOTONIC
#define NSEC_PER_SEC 1000000000

struct packet
{
    int one;
    int two;
};

msgq_t * buf = 0;
msgq_t * tick = 0;

struct timespec timespec_add(struct timespec time1, struct timespec time2)
{
    struct timespec result;
    if ((time1.tv_nsec + time2.tv_nsec) >= NSEC_PER_SEC) 
    {
        result.tv_sec = time1.tv_sec + time2.tv_sec + 1;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec - NSEC_PER_SEC;
    }
    else
    {
        result.tv_sec = time1.tv_sec + time2.tv_sec;
        result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
    }
    return result;
}

/* clock is 1KHz on RHEL5 without PREEMPT_RT */
#define PERIOD_NS 1000000

void * tick_start(void * usr)
{
    struct packet p = {0};
    struct timespec wakeupTime;
    /* 1 KHz cycle rate */
    #define CT 1000
    struct timespec cycletime = {0, PERIOD_NS};
    clock_gettime(CLOCK_TO_USE, &wakeupTime);
    int n = 0;
    while(1)
    {
        wakeupTime = timespec_add(wakeupTime, cycletime);
        clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeupTime, NULL);
        p.one++;
        //msgq_put_urgent(tick, &p, sizeof(p));
        n++;
        if(n > 1000)
        {
            printf("1s tick\n");
            n = 0;
        }
    }
}

void * send_start(void * usr)
{
    int n = 0;
    struct packet p = {0};
    while(n < 100)
    {
        p.one = n++;
        p.two = n*2;
        if((n % 2) == 0)
        {
            msgq_put(buf, &p, sizeof(p));
        }
        else
        {
            msgq_put_urgent(buf, &p, sizeof(int));
        }
    }
    printf("send done\n");
    return 0;
}

void * recv_start(void * usr)
{
    int n = 0;
    struct packet p = {0};
    while(n < 100)
    {
        p.two = 0;
        int sz = msgq_get(buf, &p, sizeof(p));
        printf("got[%d] %d, %d, %d\n", n, sz, p.one, p.two);
        n++;
    }
    printf("recv done\n");
    return 0;
}

void * tock_start(void * usr)
{
    int n = 0;
    struct packet p = {0};
    while(1)
    {
        p.two = 0;
        int sz = msgq_get(tick, &p, sizeof(p));
        n++;
    }
    return 0;
}

void set_priority(pthread_attr_t * attr, int priority)
{
    struct sched_param sched = {0};
    // 1-99, 99 is max, above 49 could starve sockets?
    // according to SOEM sample code
    sched.sched_priority = priority;
    // need EXPLICIT_SCHED or the default is
    // INHERIT_SCHED from parent process
    assert(pthread_attr_init(attr) == 0);
    assert(pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED) == 0);
    assert(pthread_attr_setschedpolicy(attr, SCHED_FIFO) == 0);
    assert(pthread_attr_setschedparam(attr, &sched) == 0);
}

int main()
{
    printf("hello\n");
    
    buf = msgq_init(16, 1000);
    tick = msgq_init(16, 1000);
    
    pthread_t thread1;
    pthread_t thread2;
    pthread_t thread3;
    pthread_t thread4;

    pthread_attr_t attr;
    set_priority(&attr, 60);

    int err;
    if((err = mlockall(MCL_CURRENT | MCL_FUTURE) != 0))
    {
        printf("warning: mlockall: %s\n", strerror(err));
    }
    
    if((err = pthread_create(&thread2, &attr, recv_start, 0)) != 0)
    {
        printf("warning: pthread_create realtime: %s\n", strerror(err));
        if((err = pthread_create(&thread2, NULL, recv_start, 0)) != 0)
        {
            printf("error: pthread_create: %s\n", strerror(err));
            exit(1);
        }
    }

    if((err = pthread_create(&thread1, &attr, send_start, 0)) != 0)
    {
        printf("warning: pthread_create realtime: %s\n", strerror(err));
        if((err = pthread_create(&thread1, NULL, send_start, 0)) != 0)
        {
            printf("error: pthread_create: %s\n", strerror(err));
            exit(1);
        }
    }
    
    if((err = pthread_create(&thread3, &attr, tick_start, 0)) != 0)
    {
        printf("warning: pthread_create realtime: %s\n", strerror(err));
        if((err = pthread_create(&thread3, NULL, tick_start, 0)) != 0)
        {
            printf("error: pthread_create: %s\n", strerror(err));
            exit(1);
        }
    }

    if((err = pthread_create(&thread4, &attr, tock_start, 0)) != 0)
    {
        printf("warning: pthread_create realtime: %s\n", strerror(err));
        if((err = pthread_create(&thread4, NULL, tock_start, 0)) != 0)
        {
            printf("error: pthread_create: %s\n", strerror(err));
            exit(1);
        }
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    pause();

    return 0;
}
