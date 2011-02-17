#include <time.h>
#include <stdint.h>
#include "rtutils.h"
#include "timer.h"
#include "messages.h"

void timer_task(void * usr)
{
    timer_usr * t = (timer_usr *)usr;
    struct timespec wakeupTime;
    struct timespec cycletime = {0, t->period_ns};
    int msg[4] = { MSG_TICK };
    clock_gettime(CLOCK_MONOTONIC, &wakeupTime);
    while(1)
    {
        wakeupTime = timespec_add(wakeupTime, cycletime);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeupTime, NULL);
        msg[1] = wakeupTime.tv_sec;
        msg[2] = wakeupTime.tv_nsec;
        rtMessageQueueSendPriority(t->sink, msg, sizeof(msg));
    }
}
