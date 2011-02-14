#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define CLOCK_TO_USE CLOCK_MONOTONIC
#define NSEC_PER_SEC (1000000000L)
#define PERIOD_NS    (500000000L)
#define BUFSIZE 1000

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

/* timer object */

struct timer_t
{
    struct timespec cycletime;
    pthread_t thread;
    struct worker_t * worker;
};

/* worker object */

struct worker_t
{
    int flag;
    pthread_t thread;
    pthread_mutex_t * mutex;
    pthread_cond_t * cond;
    struct buffer_t * buffer;
    struct buffer_t * output_buffer;
};

/* bounded buffer object */

struct buffer_t
{
    int capacity;
    int itemsize;
    int size;
    int head;
    int tail;
    char * data;
    pthread_mutex_t mutex;
    pthread_cond_t notempty;
    pthread_cond_t notfull;
};

/* buffer message object */

struct msg_t
{
    int x;
    int y;
};

void buffer_get(struct buffer_t * buffer, void * data, int locked);
void buffer_put(struct buffer_t * buffer, void * data);

void * timer_start(void * usr)
{
    struct timer_t * timer = (struct timer_t *)usr;
    struct timespec wakeupTime;
    int n = 0;
    clock_gettime(CLOCK_TO_USE, &wakeupTime);
    while(1)
    {
        wakeupTime = timespec_add(wakeupTime, timer->cycletime);
        clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeupTime, NULL);
        printf("tick\n");
        pthread_mutex_lock(timer->worker->mutex);
        timer->worker->flag = ++n;
        pthread_cond_broadcast(timer->worker->cond);
        pthread_mutex_unlock(timer->worker->mutex);
    }
    return 0;
}

void * worker_start(void * usr)
{
    struct worker_t * worker = (struct worker_t *)usr;
    struct msg_t msg = {0};
    while(1)
    {
        pthread_mutex_lock(worker->mutex);

        // spurious wakeups are possible
        while(worker->flag == 0 && worker->buffer->size == 0)
        {
            pthread_cond_wait(worker->cond, worker->mutex);
        }
        
        if(worker->flag != 0)
        {
            // timer event
            int val = worker->flag;
            worker->flag = 0;
            pthread_mutex_unlock(worker->mutex);
            printf("tock %d\n", val);
            // return updates to client
            buffer_put(worker->output_buffer, &msg);
        }
        else
        {
            // buffer event
            // get without locking or unlocking mutex
            buffer_get(worker->buffer, &msg, 1);
            pthread_mutex_unlock(worker->mutex);
            printf("fib %d\n", msg.x);
        }
    }
    return 0;
}

void buffer_get(struct buffer_t * buffer, void * data, int locked)
{
    if(!locked)
    {
        pthread_mutex_lock(&buffer->mutex);
    }
    while(buffer->size == 0)
    {
        pthread_cond_wait(&buffer->notempty, &buffer->mutex);
    }
    memcpy(data, buffer->data + buffer->tail * buffer->itemsize,
           buffer->itemsize);
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->size--;
    pthread_cond_broadcast(&buffer->notfull);
    if(!locked)
    {
        pthread_mutex_unlock(&buffer->mutex);
    }
}

void buffer_put(struct buffer_t * buffer, void * data)
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

void * consumer_start(void * usr)
{
    struct buffer_t * buffer = (struct buffer_t *) usr;
    struct msg_t msg;
    while(1)
    {
        buffer_get(buffer, &msg, 0);
        printf("reply %d\n", msg.x);
    }
    return 0;
}

int main()
{
    struct timer_t timer = {0, PERIOD_NS};
    struct worker_t worker = {0};

    struct buffer_t buffer = {BUFSIZE, sizeof(struct msg_t)};
    struct buffer_t output_buffer = {BUFSIZE, sizeof(struct msg_t)};

    timer.worker = &worker;
    worker.buffer = &buffer;
    worker.output_buffer = &output_buffer;

    buffer.data = (char *)malloc(BUFSIZE * buffer.itemsize);
    pthread_cond_init(&buffer.notfull, NULL);
    pthread_cond_init(&buffer.notempty, NULL);
    pthread_mutex_init(&buffer.mutex, NULL);
    
    output_buffer.data = (char *)malloc(BUFSIZE * output_buffer.itemsize);
    pthread_cond_init(&output_buffer.notfull, NULL);
    pthread_cond_init(&output_buffer.notempty, NULL);
    pthread_mutex_init(&output_buffer.mutex, NULL);
    
    /* can only wait on a single event so share the condition */
    worker.mutex = &buffer.mutex;
    worker.cond = &buffer.notempty;
    
    pthread_create(&timer.thread, NULL, timer_start, &timer);
    pthread_create(&worker.thread, NULL, worker_start, &worker);

    pthread_t consumer_thread;
    pthread_create(&consumer_thread, NULL, consumer_start, &output_buffer);
    
    int n0 = 1;
    int n1 = 1;
    
    while(1)
    {
        struct msg_t msg = {0};
        usleep(100000);
        if(n0 > 1000)
        {
            n0 = 1;
            n1 = 1;
        }
        msg.x = n0 + n1;
        n0 = n1;
        n1 = msg.x;
        buffer_put(&buffer, &msg);
    }

    pause();
    return 0;
}

