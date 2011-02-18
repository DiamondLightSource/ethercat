#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsMessageQueue.h>
#include <epicsExport.h>
#include <gpHash.h>
#include <iocsh.h>

#include "asynPortDriver.h"
#include "messages.h"

// Mock scanner for testing ASYN

#include "scanmock.h"

#include "/home/jr76/ethercat/scanner_complete/misc/utils/src/msgsock.h"

#define MAKE_DISPATCH_HELPER(cls, func)  \
    static void cls##_##func##_start(void * usr) \
    { \
        cls * port = static_cast<cls *>(usr); \
        port->func(); \
    }

MAKE_DISPATCH_HELPER(ScanMock, scanner);
MAKE_DISPATCH_HELPER(ScanMock, timer);
MAKE_DISPATCH_HELPER(ScanMock, reader);

ScanMock::ScanMock(const char * socket_name) : socket_name(socket_name)
{
    printf("Scanner Mockup %s\n", socket_name);
    commandq = epicsMessageQueueCreate(10, MAX_MESSAGE);

    scanner_thread = epicsThreadCreate(
        "scanner", 0, epicsThreadGetStackSize(epicsThreadStackMedium), ScanMock_scanner_start, this);
    timer_thread = epicsThreadCreate(
        "timer", 0, epicsThreadGetStackSize(epicsThreadStackMedium), ScanMock_timer_start, this);
    reader_thread = epicsThreadCreate(
        "reader", 0, epicsThreadGetStackSize(epicsThreadStackMedium), ScanMock_reader_start, this);
}

// socket to message queue adapter
void ScanMock::reader()
{
    int server_sock = rtServerSockCreate(socket_name);
    printf("create server sock %s %d\n", socket_name, server_sock);
    while(1)
    {
        int sock = rtServerSockAccept(server_sock);
        if(sock < 0)
        {
            printf("can't accept client %s\n", strerror(errno));
        }
        int tag[2] = { MSG_CONNECT, sock };
        epicsMessageQueueSend(commandq, tag, sizeof(tag));
        while(1)
        {
            char msg[MAX_MESSAGE];
            int sz = rtSockReceive(sock, msg, sizeof(msg));
            if(sz < 0)
            {
                printf("client disconnect %d\n", sz);
                break;
            }
            epicsMessageQueueSend(commandq, msg, sz);
        }
        close(sock);
    }
}
void ScanMock::timer()
{
    int msg[1] = { MSG_TICK };
    while(1)
    {
        epicsMessageQueueSend(commandq, msg, sizeof(msg));
        usleep(100000);
    }
}

struct monitor_request_node
{
    ELLNODE node;
    monitor_request req;
};

void ScanMock::scanner()
{
    char msg[MAX_MESSAGE];
    int * tag = (int *)msg;
    monitor_request * m = (monitor_request *)msg;

    ELLLIST monitors;
    ellInit(&monitors);

    int value = 0;
    int sock = 0;

    while(1)
    {
        epicsMessageQueueReceive(commandq, msg, sizeof(msg));
        if(tag[0] == MSG_CONNECT)
        {
            sock = tag[1];
        }
        else if(tag[0] == MSG_MONITOR)
        {
            printf("subscribe (%d) %d:%s\n", m->routing, m->vaddr, m->usr);
            monitor_request_node * next = 
                (monitor_request_node *)calloc(1, sizeof(monitor_request_node));
            memcpy(&next->req, m, sizeof(monitor_request_node));
            ellAdd(&monitors, &next->node);
        }
        else if(tag[0] == MSG_WRITE)
        {
            write_request * w = (write_request *)msg;
            printf("writing %d:%s %d\n", w->vaddr, w->usr, w->value);

        }
        else if(tag[0] == MSG_TICK)
        {
            monitor_request_node * node;
            for(node = (monitor_request_node *)ellFirst(&monitors); 
                node != NULL; 
                node = (monitor_request_node *)ellNext(&node->node))
            {
                monitor_response resp;
                resp.tag = MSG_REPLY;
                resp.vaddr = node->req.vaddr;
                resp.routing = node->req.routing;
                resp.value = value++;
                if(sock != 0)
                {
                    rtSockSend(sock, &resp, sizeof(resp));
                }
            }
        }
    }
}


