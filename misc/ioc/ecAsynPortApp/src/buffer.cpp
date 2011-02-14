#include "buffer.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int buffer_get(buffer_t * buffer, void * data);
void buffer_put(buffer_t * buffer, void * data);
int buffer_tryput(buffer_t * buffer, void * data);
void buffer_urgent(buffer_t * buffer, int msg);
void local_buffer_get(buffer_t * buffer, void * data);
void local_buffer_put(buffer_t * buffer, void * data);

void buffer_init(buffer_t * buffer, size_t itemsize, size_t capacity)
{
    buffer->itemsize = itemsize;
    buffer->capacity = capacity;
    buffer->data = (char *)malloc(capacity * itemsize);
    pthread_cond_init(&buffer->notfull, NULL);
    pthread_cond_init(&buffer->notempty, NULL);
    pthread_mutex_init(&buffer->mutex, NULL);
}

int buffer_get(buffer_t * buffer, void * data)
{
    pthread_mutex_lock(&buffer->mutex);
    while(buffer->size == 0 && buffer->urgent == 0)
    {
        pthread_cond_wait(&buffer->notempty, &buffer->mutex);
    }
    /* urgent message - TODO VxWorks style add to head of queue instead? */
    if(buffer->urgent == 1)
    {
        int msg = buffer->urgent;
        buffer->urgent = 0;
        pthread_mutex_unlock(&buffer->mutex);
        return msg;
    }
    memcpy(data, buffer->data + buffer->tail * buffer->itemsize,
           buffer->itemsize);
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->size--;
    pthread_cond_broadcast(&buffer->notfull);
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

void local_buffer_get(buffer_t * buffer, void * data)
{
    assert(buffer->size != 0);
    memcpy(data, buffer->data + buffer->tail * buffer->itemsize,
           buffer->itemsize);
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->size--;
}

void buffer_urgent(buffer_t * buffer, int msg)
{
    pthread_mutex_lock(&buffer->mutex);
    buffer->urgent = msg;
    pthread_cond_broadcast(&buffer->notempty);
    pthread_mutex_unlock(&buffer->mutex);
}

void buffer_put(buffer_t * buffer, void * data)
{
    pthread_mutex_lock(&buffer->mutex);
    while(buffer->size == buffer->capacity)
    {
        pthread_cond_wait(&buffer->notfull, &buffer->mutex);
    }
    memcpy(buffer->data + buffer->head * buffer->itemsize,
           data, buffer->itemsize);
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->size++;
    pthread_cond_broadcast(&buffer->notempty);
    pthread_mutex_unlock(&buffer->mutex);
}

int buffer_tryput(buffer_t * buffer, void * data)
{
    pthread_mutex_lock(&buffer->mutex);
    if(buffer->size == buffer->capacity)
    {
        pthread_mutex_unlock(&buffer->mutex);
        return -1;
    }
    memcpy(buffer->data + buffer->head * buffer->itemsize,
           data, buffer->itemsize);
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->size++;
    pthread_cond_broadcast(&buffer->notempty);
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

void local_buffer_put(buffer_t * buffer, void * data)
{
    assert(buffer->size != buffer->capacity);
    memcpy(buffer->data + buffer->head * buffer->itemsize,
           data, buffer->itemsize);
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->size++;
}

