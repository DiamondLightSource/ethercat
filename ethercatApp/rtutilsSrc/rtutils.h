#ifndef __rtUTILS_H__
#define __rtUTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

struct rtMessageQueueOSD;
typedef struct rtMessageQueueOSD *rtMessageQueueId;

enum { rtDefaultStackSize = 0 };

rtMessageQueueId rtMessageQueueCreate(
    unsigned int capacity,
    unsigned int maximumMessageSize);

void rtMessageQueueDestroy(
    rtMessageQueueId id);

int rtMessageQueueSend(
    rtMessageQueueId id,
    void *message,
    unsigned int messageSize);

int rtMessageQueueSendNoWait(
    rtMessageQueueId id,
    void *message,
    unsigned int messageSize);

int rtMessageQueueTrySend(
    rtMessageQueueId id,
    void *message,
    unsigned int messageSize);

int rtMessageQueueSendPriority(
    rtMessageQueueId id,
    void *message,
    unsigned int messageSize);

int rtMessageQueueReceive(
    rtMessageQueueId id,
    void *message,
    unsigned int size);

int rtMessageQueueTryReceive(
    rtMessageQueueId id,
    void *message,
    unsigned int size);

struct rtThreadOSD;
typedef struct rtThreadOSD *rtThreadId;
typedef void (*rtTHREADFUNC)(void *parm);

rtThreadId rtThreadCreate (
    const char * name, unsigned int priority, unsigned int stackSize,
    rtTHREADFUNC funptr, void * parm);

enum { NSEC_PER_SEC = 1000000000 };

struct timespec timespec_add(struct timespec a, struct timespec b);
struct timespec timespec_sub(struct timespec a, struct timespec b);


void new_timer(int period_ns, rtMessageQueueId sink, int priority, int tag);

#ifdef __cplusplus
}
#endif

#endif
