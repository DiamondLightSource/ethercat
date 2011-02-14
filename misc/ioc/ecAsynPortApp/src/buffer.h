#include <pthread.h>

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
    int urgent;
};

typedef struct buffer_t buffer_t;

int buffer_get(buffer_t * buffer, void * data);
void buffer_put(buffer_t * buffer, void * data);
int buffer_tryput(buffer_t * buffer, void * data);
void buffer_urgent(buffer_t * buffer, int msg);
void local_buffer_get(buffer_t * buffer, void * data);
void local_buffer_put(buffer_t * buffer, void * data);
void buffer_init(buffer_t * buffer, size_t itemsize, size_t capacity);
