#ifndef __MSGQ_H__
#define __MSGQ_H__

#include <pthread.h>

struct msgq_t
{
    int capacity;
    int itemsize;
    int size;
    int head;
    int tail;
    char * data;
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutexattr;
    pthread_cond_t notempty;
    pthread_cond_t notfull;
};
typedef struct msgq_t msgq_t;

int msgq_get(msgq_t * msgq, void * data, int size);
int msgq_put(msgq_t * msgq, void * data, int size);
int msgq_put_urgent(msgq_t * msgq, void * data, int size);
int msgq_tryput(msgq_t * msgq, void * data, int size);
msgq_t * msgq_init(int itemsize, int capacity);

#endif
