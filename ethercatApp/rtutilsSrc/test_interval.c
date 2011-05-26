#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "rtutils.h"

rtMessageQueueId q;

void one_start(void * usr)
{
    int n = 0;
    int msg[10] = {0};
    struct timespec wakeupTime;
    struct timespec startTime;
    struct timespec diffTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);
    while(1)
    {
        rtMessageQueueReceive(q, &msg, sizeof(msg));
        if(n == 0)
        {
            clock_gettime(CLOCK_MONOTONIC, &wakeupTime);
            diffTime = timespec_sub(wakeupTime, startTime);
            double dt = diffTime.tv_sec + diffTime.tv_nsec * 1e-9;
            printf("dt %f\n", dt);
            startTime = wakeupTime;
        }
        n = (n + 1) % 1000;
    }
}

void two_start(void * usr)
{
    int msg[10] = {0};
    struct timespec wakeupTime;
    struct timespec delay = {0, 1e5};
    clock_gettime(CLOCK_MONOTONIC, &wakeupTime);
    while(1)
    {
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeupTime, NULL);
        rtMessageQueueSend(q, &msg, sizeof(msg));
        msg[0]++;
        wakeupTime = timespec_add(wakeupTime, delay);
    }
}

int main()
{    
    rtThreadId one, two;

    q = rtMessageQueueCreate(16, 1024);
    
    one = rtThreadCreate(
        "one", 30, 
        rtDefaultStackSize,
        one_start, NULL);

    two = rtThreadCreate(
        "two", 30, 
        rtDefaultStackSize,
        two_start, NULL);

    pause();
    
    return 0;
}

/* 
   tests minimum timer interval, 1ms on RHEL5 without PREEMPT_RT
   success -> ticks are 0.1s apart
   failure -> ticks are   1s apart
*/
   
