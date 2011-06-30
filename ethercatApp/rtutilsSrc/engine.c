#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "rtutils.h"
#include "msgsock.h"

int server_connect(ENGINE * server)
{
    struct timeval tv;
    tv.tv_sec = server->timeout;
    tv.tv_usec = 0;
    int sock = rtServerSockAccept(server->listening);
    printf("CONNECTED %d %d\n", sock, server->id);
    assert(setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(struct timeval)) == 0);
    assert(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)) == 0);
    return sock;
}

int client_connect(ENGINE * client)
{
    return rtSockCreate(client->path);
}

enum { CTRL_SHUTDOWN = -100, CTRL_IDLE = -200 };

static void write_thread(void * usr)
{
    ENGINE * server = (ENGINE *)usr;
    while(1)
    {
        int msg;
        int sock;
        while(1)
        {
            // discard any shutdown messages from previous connection
            rtMessageQueueReceive(server->writectrl, &sock, sizeof(int));
            if(sock >= 0)
            {
                break;
            }
        }
        while(1)
        {
            int size = server->receive_message(server);
            assert(size >= 0);
            int status = rtSockSend(sock, server->send_buffer, size);
            if(status != 0)
            {
                printf("socket send failed %d\n", server->id);
                msg = CTRL_SHUTDOWN;
                rtMessageQueueSend(server->readctrl, &msg, sizeof(int));
                break;
            }
            int failed = rtMessageQueueTryReceive(server->writectrl, &msg, sizeof(int));
            if(failed > 0)
            {
                printf("writer interrupted %d\n", server->id);
                break;
            }
        }
        // enter idle state and signal
        msg = CTRL_IDLE;
        rtMessageQueueSend(server->readctrl, &msg, sizeof(int));
    }
}

static void read_thread(void * usr)
{
    ENGINE * server = (ENGINE *)usr;
    while(1)
    {
        int msg;
        int sock;
        // connect phase
        while(1)
        {
            sock = server->connect(server);
            if(sock < 0)
            {
                printf("connect failed\n");
                usleep(1000000);
            }
            else
            {
                break;
            }
        }
        // handshake phase
        if(server->on_connect(server, sock) != 0)
        {
            printf("handshake failed\n");
            close(sock);
            usleep(1000000);
            continue;
        }
        // streaming phase
        rtMessageQueueSend(server->writectrl, &sock, sizeof(int));
        while(1)
        {
            int size = rtSockReceive(sock, server->receive_buffer, server->max_message);
            if(size <= 0)
            {
                printf("socket receive failed %d\n", server->id);
                msg = CTRL_SHUTDOWN;
                rtMessageQueueSend(server->writectrl, &msg, sizeof(int));
                break;
            }
            server->send_message(server, size);
            int failed = rtMessageQueueTryReceive(server->readctrl, &msg, sizeof(int));
            if(failed > 0)
            {
                printf("reader interrupted %d\n", server->id);
                break;
            }
        }
        // wait for writer to enter idle state before closing socket
        while(1)
        {
            rtMessageQueueReceive(server->readctrl, &msg, sizeof(int));
            if(msg == CTRL_IDLE)
            {
                break;
            }
        }
        printf("closing socket %d\n", server->id);
        close(sock);
        usleep(1000000);
    }
}

void engine_start(ENGINE * server)
{
    rtThreadCreate("read", 0, rtDefaultStackSize, read_thread, server);
    rtThreadCreate("write", 0, rtDefaultStackSize, write_thread, server);
}

ENGINE * new_engine(int max_message)
{
    ENGINE * server = calloc(1, sizeof(ENGINE));
    server->timeout = 60;
    server->max_message = max_message;
    server->send_buffer = calloc(1, server->max_message);
    server->receive_buffer = calloc(1, server->max_message);
    server->readctrl = rtMessageQueueCreate(10, sizeof(int));
    server->writectrl = rtMessageQueueCreate(10, sizeof(int));
    return server;
}

#if 0

// TEST PROGRAM

typedef struct
{
    int max_servers;
    rtMessageQueueId * queues;
} SCANNER;

static int send_config_message(ENGINE * server, int sock)
{
    rtSockSend(sock, &server->id, sizeof(int));
    return 0;
}

static int server_receive_message(ENGINE * server)
{
    return rtMessageQueueReceive((rtMessageQueueId)server->usr, server->send_buffer, server->max_message);
}

static int count = 0;

static int client_receive_message(ENGINE * server)
{
    *((int *)server->send_buffer) = count++;
    if((count % 5) == 4)
    {
        return 0;
    }
    else
    {
        return sizeof(int);
    }
}

static void scanner_thread(void * usr)
{
    SCANNER * scanner = (SCANNER *)usr;
    int n = 66;
    while(1)
    {
        int msg[1] = {n};
        int c;
        for(c = 0; c < scanner->max_servers; c++)
        {
            //rtMessageQueueSendNoWait(scanner->servers[c].q, msg, sizeof(msg));
            rtMessageQueueSendNoWait(scanner->queues[c], msg, sizeof(msg));
        }
        usleep(100000);
        n++;
    }
}

static int hello(ENGINE * server, int size)
{
    printf("got some stuff %d\n", size);
    return 0;
}

static int client_send_message(ENGINE * client, int size)
{
    printf("pdo distribution %d\n", size);
    return 0;
}

static int client_read_config(ENGINE * client, int sock)
{
    int size = rtSockReceive(sock, client->receive_buffer, client->max_message);
    printf("CONFIG %d %d\n", size, client->receive_buffer[0]);
    return 0;
}

int socktest_test_main()
{
    int max_clients = 10;
    int max_message = 1000000;
    SCANNER * scanner = calloc(1, sizeof(SCANNER));
    scanner->max_servers = max_clients;
    scanner->queues = calloc(max_clients, sizeof(rtMessageQueueId));
    char * path = "/tmp/socket";
    int sock = rtServerSockCreate(path);
    int n;
    for(n = 0; n < max_clients; n++)
    {
        ENGINE * server = new_engine(max_message);
        server->id = n;
        server->listening = sock;
        server->on_connect = send_config_message;
        server->receive_message = server_receive_message;
        server->send_message = hello;
        server->connect = server_connect;
        scanner->queues[n] = rtMessageQueueCreate(10, sizeof(int));
        server->usr = scanner->queues[n];
        engine_start(server);
    }
    for(n = 0; n < max_clients; n++)
    {
        ENGINE * client = new_engine(max_message);
        client->id = n;
        client->path = path;
        client->on_connect = client_read_config;
        client->send_message = client_send_message;
        client->receive_message = client_receive_message;
        client->connect = client_connect;
        engine_start(client);
    }
    rtThreadCreate("scanner", 0, rtDefaultStackSize, scanner_thread, scanner);
    pause();
    return 0;
}

/*
  Timeout causes EAGAIN on send or receive
  Stevens book for socket programming (very useful)
  put in rtutils
*/

#endif
