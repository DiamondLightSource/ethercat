// Mock scanner for testing ASYN

class ScanMock
{
    epicsMessageQueueId commandq;
    const char * socket_name;
public: 
    epicsThreadId scanner_thread;
    epicsThreadId timer_thread;
    epicsThreadId reader_thread;
    ScanMock(const char * socket_name);
    void scanner();
    void timer();
    void reader();
};

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
    printf("create server sock %s\n", socket_name);
    int server_sock = rtServerSockCreate(socket_name);
    assert(server_sock);
    while(1)
    {
        int sock = rtServerSockAccept(server_sock);
        assert(sock);
        int tag[2] = { MSG_CONNECT, sock };
        epicsMessageQueueSend(commandq, tag, sizeof(tag));
        while(1)
        {
            char msg[MAX_MESSAGE];
            int sz = rtSockReceive(sock, msg, sizeof(msg));
            if(sz == -1)
            {
                printf("client disconnect\n");
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
            monitor_request * next = 
                (monitor_request *)calloc(1, sizeof(monitor_request));
            memcpy(next, m, sizeof(monitor_request));
            ellAdd(&monitors, &next->node);
        }
        else if(tag[0] == MSG_WRITE)
        {
            write_request * w = (write_request *)msg;
            printf("writing %d:%s %lld\n", w->vaddr, w->usr, w->value);

        }
        else if(tag[0] == MSG_TICK)
        {
            ELLNODE * node;
            for(node = ellFirst(&monitors); node != NULL; node = ellNext(node))
            {
                monitor_request * m = (monitor_request *)
                    ((char *)node - offsetof(monitor_request, node));
                
                monitor_response resp;
                resp.tag = MSG_REPLY;
                resp.vaddr = m->vaddr;
                resp.routing = m->routing;
                resp.value = value++;
                if(sock != 0)
                {
                    rtSockSend(sock, &resp, sizeof(resp));
                }
            }
        }
    }
}


