#include <epicsMessageQueue.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <cantProceed.h>
#include <unistd.h>
#include <stdio.h>

void epicsMessageQueueAllocateEvents(epicsMessageQueueId, int);

epicsMessageQueueId q;
epicsMessageQueueId sync0, sync1, sync2, syncm1, syncm2, syncdone;
epicsMutexId lock;
int done = 0;

void low_start(void * usr)
{
    int msg[4] = {0};
    epicsMutexLock(lock);
    // wait for medium threads
    epicsMessageQueueReceive(syncm1, &msg, sizeof(msg));
    epicsMessageQueueReceive(syncm2, &msg, sizeof(msg));
    // signal second medium thread
    epicsMessageQueueSend(sync0, &msg, sizeof(msg));
    // won't wake up because two medium threads are running
    epicsMessageQueueReceive(sync1, &msg, sizeof(msg));
    printf("low task wakes up\n");

    struct sched_param sched = {0};
    int policy = 0;
    assert(pthread_getschedparam(pthread_self(), &policy, &sched) == 0);
    printf("low: policy %d priority %d\n", 
           policy, sched.sched_priority);

    epicsMessageQueueSend(syncdone, &msg, sizeof(msg));
    epicsMutexUnlock(lock);
}

void med1_start(void * usr)
{
    int n;
    int msg[4] = {0};
    epicsMessageQueueSend(syncm1, &msg, sizeof(msg));
    while(1)
    {
        n++;
    }
}

void med2_start(void * usr)
{
    int n;
    int msg[4] = {0};
    epicsMessageQueueSend(syncm2, &msg, sizeof(msg));
    epicsMessageQueueReceive(sync0, &msg, sizeof(msg));
    printf("med wakeup\n");
    epicsMessageQueueSend(sync1, &msg, sizeof(msg));
    epicsMessageQueueSend(sync2, &msg, sizeof(msg));
    while(1)
    {
        n++;
    }
}

void high_start(void * usr)
{
    int msg[4] = {0};
    epicsMessageQueueReceive(sync2, &msg, sizeof(msg));
    printf("high wakeup\n");
    if(usr)
    {
        printf("high getting mutex held by low task\n");
        epicsMutexLock(lock);
    }
}

int main(int argc, char ** argv)
{
    epicsThreadId low, med1, med2, high;

    lock = epicsMutexCreate();
    
    q = epicsMessageQueueCreate(16, 1024);
    sync0 = epicsMessageQueueCreate(16, 1024);
    sync1 = epicsMessageQueueCreate(16, 1024);
    sync2 = epicsMessageQueueCreate(16, 1024);
    syncm1 = epicsMessageQueueCreate(16, 1024);
    syncm2 = epicsMessageQueueCreate(16, 1024);    
    syncdone = epicsMessageQueueCreate(16, 1024);
    
    printf("EPICS priority inheritance test\n");
    int hold = 1;

    epicsMessageQueueAllocateEvents(q, 128);
    
    low = epicsThreadCreate(
        "low", 10, 
        epicsThreadGetStackSize(epicsThreadStackBig), 
        low_start, NULL);

    med1 = epicsThreadCreate(
        "med1", 30, 
        epicsThreadGetStackSize(epicsThreadStackBig),
        med1_start, NULL);

    med2 = epicsThreadCreate(
        "med2", 30, 
        epicsThreadGetStackSize(epicsThreadStackBig),
        med2_start, NULL);

    high = epicsThreadCreate(
        "high", 60, 
        epicsThreadGetStackSize(epicsThreadStackBig), 
        high_start, (void *)hold);

    int msg[4] = {0};

    epicsMessageQueueReceive(syncdone, &msg, sizeof(msg));
    
    printf("test complete\n");

    return 0;
}
   
/* 
   don't run this with hold=0 on PREEMPT_RT unless you
   chrt -f 90 your sshd and all subprocesses first 
*/

