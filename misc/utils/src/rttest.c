#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>

#include "msgsock.h"
#include "msgq.h"

enum tags { TICK, WORK, DONE };
char * tag_name[] =  { "TICK", "WORK", "DONE" };

msgq_t * replyq = NULL;
msgq_t * clientq = NULL;

void * socket_start(void * usr)
{
    uint32_t msg[2] = {0, 0};
    while(1)
    {
        msgq_get(clientq, msg, sizeof(msg));
        printf("data %d %d\n", msg[0], msg[1]);
        usleep(100000);
    }
    return 0;
}

void * writer_start(void * usr)
{
    uint32_t msg[2] = {WORK, 10};
    while(1)
    {
        msgq_put(replyq, msg, sizeof(msg));
    }
    return 0;
}

void * reader_start(void * usr)
{
    /* real time thread */
    uint32_t buf[16];
    int n = 0;
    int a = 0;
    while(1)
    {
        /* ok to block for work here */
        msgq_get(replyq, buf, sizeof(buf));
        if(buf[0] == TICK)
        {
            printf("%s %d\n", tag_name[buf[0]], buf[1]);
        }
        buf[0] = a;
        buf[1] = n;
        int r = msgq_tryput(clientq, buf, sizeof(buf));
        if(r < 0)
        {
            n++;
        }
        else
        {
            a++;
        }
    }
}

void * timer_start(void * usr)
{
    uint32_t period = 100000;
    uint32_t msg[2] = {TICK, period};
    while(1)
    {
        usleep(period);
        msgq_put_urgent(replyq, &msg, sizeof(msg));
    }
}

void init_priority(pthread_attr_t * attr, int priority)
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

int main(void)
{
    replyq = msgq_init(1024, 1024);
    clientq = msgq_init(1024, 5);

    pthread_t reader_thread;
    pthread_t writer_thread;
    pthread_t timer_thread;
    pthread_t socket_thread;

    pthread_attr_t high;
    init_priority(&high, 60);
    
    pthread_create(&timer_thread,  &high, timer_start,  NULL);
    pthread_create(&reader_thread, &high, reader_start, NULL);
    pthread_create(&writer_thread, NULL,  writer_start, NULL);
    pthread_create(&socket_thread, NULL,  socket_start, NULL);
    
    pause();
    
    return 0;
}

/* 

Timer task blocking
===================

Work queue is full. Worker wakes up, takes data.
Timer task has priority, puts data in queue AT HEAD.
This is OK. No work messages are deleted.
Timer is not blocked by low priority task, timer is blocked by worker task. That's ok.

Real-time task block
====================

Reply queue is full.
Don't overwrite old data, discard new messages.
Clients must survive missing messages from the RT task.


*/
