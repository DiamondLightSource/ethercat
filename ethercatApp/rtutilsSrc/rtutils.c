#define _GNU_SOURCE

#include <time.h>
#include "rtutils.h"
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define TAILPTR(q) (unsigned *)(q->data + q->tail * \
                                (q->maximumMessageSize + sizeof(int)))
#define HEADPTR(q) (unsigned *)(q->data + q->head * \
                                (q->maximumMessageSize + sizeof(int)))


typedef struct
{
    int tag;
    struct timespec ts;
} TIMER_MESSAGE;

struct rtMessageQueueOSD
{
    int capacity;
    int maximumMessageSize;
    int length;
    int head;
    int tail;
    char * data;
    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;
    pthread_cond_t notempty;
    pthread_cond_t notfull;
};

struct rtThreadOSD
{
    pthread_t thread;
    pthread_attr_t attr;
    void * usr;
    rtTHREADFUNC start;
};

static void * start_routine(void *arg)
{
    rtThreadId thread = (rtThreadId)arg;
    thread->start(thread->usr);
    return NULL;
}

rtThreadId rtThreadCreate (
    const char * name, unsigned int priority, unsigned int stackSize,
    rtTHREADFUNC funptr, void * parm)
{
    struct sched_param sched = {0};
    rtThreadId thread = 
        (rtThreadId)calloc(1, sizeof(struct rtThreadOSD));
    assert(thread != NULL);
    sched.sched_priority = priority;
    assert(pthread_attr_init(&thread->attr) == 0);
    if(priority)
    {
        assert(pthread_attr_setinheritsched(&thread->attr,
                                            PTHREAD_EXPLICIT_SCHED) == 0);
        assert(pthread_attr_setschedpolicy(&thread->attr,
                                           SCHED_FIFO) == 0);
        assert(pthread_attr_setschedparam(&thread->attr,
                                          &sched) == 0);
    }
    thread->start = funptr;
    thread->usr = parm;
    int result = pthread_create(&thread->thread, &thread->attr, start_routine, thread);
    if( result == 0)
    {
        return thread;
    }
    else
    {
        switch(result)
        {
        case EAGAIN: printf("rtThreadCreate: EAGAIN (%d)\n", result); 
            break;
        case EINVAL: printf("rtThreadCreate: EINVAL (%d)\n", result); 
            break;
        case EPERM: printf("rtThreadCreate: EPERM (%d)\n", result); 
            break;
        default:
            printf("rtThreadCreate: other error %d\n", result ); 
            break;
        }
        return NULL;
    }
}

rtMessageQueueId rtMessageQueueCreate(
    unsigned int capacity,
    unsigned int maximumMessageSize)
{
    rtMessageQueueId q = 
        (rtMessageQueueId)calloc(1, sizeof(struct rtMessageQueueOSD));
    assert(q != NULL);
    q->capacity = capacity;
    q->maximumMessageSize = maximumMessageSize;
    q->length = 0;
    /* allocate extra space for message size */
    q->data = (char *)malloc(capacity * (maximumMessageSize + sizeof(int)));
    assert(q->data != NULL);
    assert(pthread_mutexattr_init(&q->attr) == 0);
    assert(pthread_mutexattr_setprotocol(&q->attr,
                                         PTHREAD_PRIO_INHERIT) == 0);
    assert(pthread_mutex_init(&q->mutex, &q->attr) == 0);
    assert(pthread_cond_init(&q->notfull, NULL) == 0);
    assert(pthread_cond_init(&q->notempty, NULL) == 0);
    return q;
}

int rtMessageQueueReceive(
    rtMessageQueueId q,
    void *data,
    unsigned int size)
{
    pthread_mutex_lock(&q->mutex);
    while(q->length == 0)
    {
        pthread_cond_wait(&q->notempty, &q->mutex);
    }
    unsigned int * slot = TAILPTR(q);
    if(*slot < size)
    {
        size = *slot;
    }
    memcpy(data, slot + 1, size);
    q->tail = (q->tail + 1) % q->capacity;
    q->length--;
    pthread_cond_broadcast(&q->notfull);
    pthread_mutex_unlock(&q->mutex);
    return size;
}

int rtMessageQueueTryReceive(
    rtMessageQueueId q,
    void *data,
    unsigned int size)
{
    pthread_mutex_lock(&q->mutex);
    while(q->length == 0)
    {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }
    unsigned int * slot = TAILPTR(q);
    if(*slot < size)
    {
        size = *slot;
    }
    memcpy(data, slot + 1, size);
    q->tail = (q->tail + 1) % q->capacity;
    q->length--;
    pthread_cond_broadcast(&q->notfull);
    pthread_mutex_unlock(&q->mutex);
    return size;
}

static int msgq_put_(rtMessageQueueId msgq, void * data, int size, int nowait)
{
    if(size > msgq->maximumMessageSize)
    {
        return -1;
    }
    pthread_mutex_lock(&msgq->mutex);
    /* remove oldest element */
    if(nowait && msgq->length == msgq->capacity)
    {
        msgq->tail = (msgq->tail + 1) % msgq->capacity;
        msgq->length--;
    }
    while(msgq->length == msgq->capacity)
    {
        pthread_cond_wait(&msgq->notfull, &msgq->mutex);
    }
    unsigned int * slot = HEADPTR(msgq);
    *slot = size;
    memcpy(slot + 1, data, size);
    msgq->head = (msgq->head + 1) % msgq->capacity;
    msgq->length++;
    pthread_cond_broadcast(&msgq->notempty);
    pthread_mutex_unlock(&msgq->mutex);
    return 0;
}

int rtMessageQueueSend(rtMessageQueueId msgq, void * data, unsigned int size)
{
    return msgq_put_(msgq, data, size, 0);
}

int rtMessageQueueTrySend(rtMessageQueueId msgq, void * data, unsigned int size)
{
    return msgq_put_(msgq, data, size, 1);
}

int rtMessageQueueSendNoWait(rtMessageQueueId msgq, void * data, unsigned int size)
{
    return msgq_put_(msgq, data, size, 1);
}

int rtMessageQueueSendPriority(rtMessageQueueId msgq, void * data, unsigned int size)
{
    if(size > msgq->maximumMessageSize)
    {
        return -1;
    }
    pthread_mutex_lock(&msgq->mutex);
    while(msgq->length == msgq->capacity)
    {
        pthread_cond_wait(&msgq->notfull, &msgq->mutex);
    }
    msgq->tail = (msgq->tail - 1);
    // mod (%) doesn't work with negative numbers
    if(msgq->tail < 0)
    {
        msgq->tail = msgq->capacity - 1;
    }
    unsigned int * slot = TAILPTR(msgq);
    *slot = size;
    memcpy(slot + 1, data, size);
    msgq->length++;
    pthread_cond_broadcast(&msgq->notempty);
    pthread_mutex_unlock(&msgq->mutex);
    return 0;
}

struct timespec timespec_add(struct timespec a, struct timespec b)
{
    struct timespec result;
    result.tv_nsec = a.tv_nsec + b.tv_nsec;
    result.tv_sec  = a.tv_sec  + b.tv_sec;
    while(result.tv_nsec >= NSEC_PER_SEC)
    {
        result.tv_nsec -= NSEC_PER_SEC;
        result.tv_sec  += 1;
    }
    return result;
}

struct timespec timespec_sub(struct timespec a, struct timespec b)
{
    struct timespec result;
    result.tv_nsec = a.tv_nsec - b.tv_nsec;
    result.tv_sec  = a.tv_sec  - b.tv_sec;
    while(result.tv_nsec < 0)
    {
        result.tv_nsec += NSEC_PER_SEC;
        result.tv_sec  -= 1;
    }
    return result;
}

typedef struct
{
    int tag;
    int period_ns;
    rtMessageQueueId sink;
} TIMER;

static void timer_task(void * usr)
{
    TIMER * timer = (TIMER *)usr;
    struct timespec wakeupTime = {0};
    struct timespec cycletime = {0};
    cycletime.tv_nsec = timer->period_ns;
    TIMER_MESSAGE msg;
    msg.tag = timer->tag;
    clock_gettime(CLOCK_MONOTONIC, &wakeupTime);
    while(1)
    {
        wakeupTime = timespec_add(wakeupTime, cycletime);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeupTime, NULL);
        msg.ts = wakeupTime;
        rtMessageQueueSendPriority(timer->sink, &msg, sizeof(TIMER_MESSAGE));
    }
}

void new_timer(int period_ns, rtMessageQueueId sink, int priority, int tag)
{
    TIMER * timer = calloc(1, sizeof(TIMER));
    timer->period_ns = period_ns;
    timer->sink = sink;
    timer->tag = tag;
    printf("Creating new timer with period %d nanoseconds\n", period_ns);
    rtThreadCreate("timer", priority, 0, timer_task, timer);
}
