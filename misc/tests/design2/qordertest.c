// queue overflow in-order test

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "rtutils.h"

rtMessageQueueId clientq;

#define MAXN 100000000

void reader_task(void * usr)
{
    int last = 0;
    int msg[1];
    while(last < MAXN-1)
    {
        usleep(1000);
        assert(rtMessageQueueReceive(clientq, msg, sizeof(msg)) == sizeof(msg));
        printf("%d\n", msg[0]);
        assert(msg[0] > last);
        last = msg[0];
    }
    printf("done\n");
    exit(0);
}

void writer_task(void * usr)
{
    int msg[1];
    int n;
    for(n = 0; n < MAXN; n++)
    {
        msg[0] = n;
        assert(rtMessageQueueSendNoWait(clientq, msg, sizeof(msg)) == 0);
    }
}

int main()
{
    clientq = rtMessageQueueCreate(10, sizeof(int));
    rtThreadCreate("reader", 0,  0, reader_task, NULL );
    rtThreadCreate("reader", 0,  0, writer_task, NULL );
    pause();
    
    return 0;
}
