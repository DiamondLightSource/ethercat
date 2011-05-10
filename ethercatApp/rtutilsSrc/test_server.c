#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <assert.h>

#include "msgsock.h"
#include "rtutils.h"

char * socket_name = "/tmp/sock";

rtMessageQueueId replyq;

struct cmd
{
    int value[4];
};

void writer_start(void * usr)
{
    int fd = (int)usr;
    struct cmd packet = {{0}};
    struct cmd packet1 = {{0}};
    while(1)
    {
        int status = rtSockSend(fd, &packet, sizeof(packet));
        if(status != 0)
        {
            printf("writer abort: %d\n", status);
            break;
        }
        usleep(1000);
        rtMessageQueueReceive(replyq, &packet1, sizeof(packet1));
        printf("%d %d\n", packet.value[0], packet1.value[1]);
        packet.value[0]++;
    }
}

void reader_start(void * usr)
{
    int fd = (int)usr;
    struct cmd packet = {{0}};
    while(1)
    {
        int sz = rtSockReceive(fd, &packet, sizeof(packet));
        if(sz <= 0)
        {
            printf("reader abort: %d\n", sz);
            break;
        }
        printf("reply size %d: %d %d\n", sz, packet.value[0], packet.value[1]);
        rtMessageQueueSend(replyq, &packet, sizeof(packet));
    }
}

/* echo + multiplication server */
void * handle_conn(int sock, void * usr)
{
    int msg[1024];
    while(1)
    {
        int n = rtSockReceive(sock, (char *)&msg, sizeof(msg));
        if(n <= 0)
        {
            printf("disconnect by %d\n", n);
            break;
        }
        
        printf("got message, size %d, payload %d %d %d %d\n", n, msg[0], msg[1], msg[2], msg[3]);
        
        msg[1] = msg[0] * 10;

        if(rtSockSend(sock, (char *)&msg, n) != 0)
        {
            printf("send failed\n");
        }
    }
    
    close(sock);
    return 0;
}

void timer_start(void * usr)
{
    int n = 0;
    while(1)
    {
        usleep(1000000);
        for(n = 0; n < 10000; n++)
        {
            printf("tick\n");
        }
    }
}

/* round trip test with reader and writer thread, message queue */
void server_start(void * usr)
{
    rtMessageQueueId q = (rtMessageQueueId)usr;
    int sock = rtServerSockCreate(socket_name);
    rtMessageQueueSend(q, &sock, sizeof(sock));
    while(1)
    {
        int c = rtServerSockAccept(sock);
        handle_conn(c, NULL);
    }
}

int main(void)
{
    
    int msg;
    
    rtMessageQueueId q = rtMessageQueueCreate(1, sizeof(msg));

    rtThreadId server_thread = rtThreadCreate(
        "reader", 0, rtDefaultStackSize, server_start, q);
    assert(server_thread);

    rtMessageQueueReceive(q, &msg, sizeof(msg));

    printf("got %d\n", msg);
    
    int sock = rtSockCreate(socket_name);
    
    replyq = rtMessageQueueCreate(1024, 1024);

    rtThreadId reader_thread = rtThreadCreate(
        "reader", 0, rtDefaultStackSize, reader_start,
        (void *)sock);
    assert(reader_thread);
    rtThreadId writer_thread = rtThreadCreate(
        "writer", 0, rtDefaultStackSize, writer_start,
        (void *)sock);
    assert(writer_thread);
    rtThreadId timer_thread = rtThreadCreate(
        "timer", 0, rtDefaultStackSize, timer_start, NULL);
    assert(timer_thread);
    
    pause();
    
    close(sock);
    
    return 0;
}

// timer thread, network thread ok
