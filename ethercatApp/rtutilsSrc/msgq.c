/* enable unix98 features such as PTHREAD_PRIO_INHERIT */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

#include "msgq.h"

#define TAILPTR(b) (int *)(b->data + b->tail * (b->itemsize + sizeof(int)))
#define HEADPTR(b) (int *)(b->data + b->head * (b->itemsize + sizeof(int)))

int msgq_get(msgq_t * msgq, void * data, int size)
{
    pthread_mutex_lock(&msgq->mutex);
    while(msgq->size == 0)
    {
        pthread_cond_wait(&msgq->notempty, &msgq->mutex);
    }
    int * slot = TAILPTR(msgq);
    if(*slot < size)
    {
        size = *slot;
    }
    memcpy(data, slot + 1, size);
    msgq->tail = (msgq->tail + 1) % msgq->capacity;
    msgq->size--;
    pthread_cond_broadcast(&msgq->notfull);
    pthread_mutex_unlock(&msgq->mutex);
    return size;
}

static int msgq_put_(msgq_t * msgq, void * data, int size, int nowait)
{
    if(size > msgq->itemsize)
    {
        return -2;
    }
    pthread_mutex_lock(&msgq->mutex);
    if(msgq->size == msgq->capacity && nowait)
    {
        pthread_mutex_unlock(&msgq->mutex);
        return -1;
    }
    while(msgq->size == msgq->capacity)
    {
        pthread_cond_wait(&msgq->notfull, &msgq->mutex);
    }
    int * slot = HEADPTR(msgq);
    *slot = size;
    memcpy(slot + 1, data, size);
    msgq->head = (msgq->head + 1) % msgq->capacity;
    msgq->size++;
    pthread_cond_broadcast(&msgq->notempty);
    pthread_mutex_unlock(&msgq->mutex);
    return 0;
}

int msgq_put(msgq_t * msgq, void * data, int size)
{
    return msgq_put_(msgq, data, size, 0);
}

int msgq_tryput(msgq_t * msgq, void * data, int size)
{
    return msgq_put_(msgq, data, size, 1);
}

int msgq_put_urgent(msgq_t * msgq, void * data, int size)
{
    if(size > msgq->itemsize)
    {
        return -1;
    }
    pthread_mutex_lock(&msgq->mutex);
    while(msgq->size == msgq->capacity)
    {
        pthread_cond_wait(&msgq->notfull, &msgq->mutex);
    }
    msgq->tail = (msgq->tail - 1);
    // mod (%) doesn't work with negative numbers
    if(msgq->tail < 0)
    {
        msgq->tail = msgq->capacity - 1;
    }
    int * slot = TAILPTR(msgq);
    *slot = size;
    memcpy(slot + 1, data, size);
    msgq->size++;
    pthread_cond_broadcast(&msgq->notempty);
    pthread_mutex_unlock(&msgq->mutex);
    return 0;
}

msgq_t * msgq_init(int itemsize, int capacity)
{
    msgq_t * msgq = (msgq_t *)calloc(1, sizeof(msgq_t));
    msgq->size = 0;
    msgq->itemsize = itemsize;
    msgq->capacity = capacity;
    /* allocate extra space for message size */
    msgq->data = (char *)malloc(capacity * (itemsize + sizeof(int)));
    assert(pthread_mutexattr_init(&msgq->mutexattr) == 0);
    assert(pthread_mutexattr_setprotocol(&msgq->mutexattr, 
                                         PTHREAD_PRIO_INHERIT) == 0);
    assert(pthread_mutex_init(&msgq->mutex, &msgq->mutexattr) == 0);
    assert(pthread_cond_init(&msgq->notfull, NULL) == 0);
    assert(pthread_cond_init(&msgq->notempty, NULL) == 0);
    return msgq;
}
