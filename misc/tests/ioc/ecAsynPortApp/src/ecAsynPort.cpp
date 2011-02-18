#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

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
#include "scanmock.h"

#include "/home/jr76/ethercat/scanner_complete/misc/utils/src/msgsock.h"
#include "/home/jr76/ethercat/scanner_complete/misc/utils/src/msgsock.c"

#define MAKE_DISPATCH_HELPER(cls, func)  \
    static void cls##_##func##_start(void * usr) \
    { \
        cls * port = static_cast<cls *>(usr); \
        port->func(); \
    }

class ecAccumulate : public asynPortDriver 
{
    int length;
    int ofs;
    int delay;
    int triggered;
    int period;
    int tick;
    int countdown;
    int32_t * buffer;
    int P_Buffer;
    int P_Trigger;
public:
    ecAccumulate(const char * portName, const char * ecPortName, 
                 int addr, const char * name, int length, int delay, int period);
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 * value);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                        size_t nElements, size_t *nIn);
    void interrupt_callback(asynUser *pasynUser, epicsInt32 value);
    void doCallback();
};

asynStatus ecAccumulate::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    // TODO no synchronization with the interrupt task
    // but you get the idea
    if(pasynUser->reason == P_Trigger)
    {
        if(value == 0)
        {
            triggered = 0;
        }
        if(value == 1)
        {
            triggered = 1;
        }
    }
    return asynSuccess;
}

asynStatus ecAccumulate::readInt32(asynUser *pasynUser, epicsInt32 * value)
{
    *value = 0;
    return asynSuccess;
}

void ecAccumulate::doCallback()
{
    ELLLIST *pclientList;
    interruptNode *node;
    asynStandardInterfaces *pInterfaces = &asynStdInterfaces;
    assert(pInterfaces->int32ArrayInterruptPvt);
    pasynManager->interruptStart(pInterfaces->int32ArrayInterruptPvt, &pclientList);
    for(node = (interruptNode *)ellFirst(pclientList); 
        node != NULL; 
        node = (interruptNode *)ellNext(&node->node))
    {
        asynInt32ArrayInterrupt *pInterrupt = (asynInt32ArrayInterrupt *) node->drvPvt;
        pInterrupt->callback(pInterrupt->userPvt, pInterrupt->pasynUser, buffer, length);
    }
    pasynManager->interruptEnd(pInterfaces->int32ArrayInterruptPvt);
}

void ecAccumulate::interrupt_callback(asynUser *pasynUser, epicsInt32 value)
{
    // TODO no synchronization with the trigger write

    int running = 1;
    if(triggered == 0)
    {
        countdown = delay;
        running = 1;
    }
    else if(countdown > 0)
    {
        countdown--;
        running = 1;
    }
    else
    {
        running = 0;
    }

    if(running)
    {
        buffer[ofs] = value;
        ofs = (ofs + 1) % length;

        if(tick == 0)
        {
            doCallback();
        }
        tick = (tick + 1) % period;
    }
    
}

void queue_request_helper(asynUser *pasynUser)
{
    // ASYN port queue request callback - not used
}

void interrupt_callback_helper(void *drvPvt, asynUser *pasynUser,
                               epicsInt32 value)
{
    ecAccumulate * acc = (ecAccumulate *)drvPvt;
    acc->interrupt_callback(pasynUser, value);
}

ecAccumulate::ecAccumulate(const char *portName, const char * ecPortName,
                           int addr, const char * name, int length, int delay, int period)
    : asynPortDriver(portName,
                     1, /* maxAddr - not using C++ params */
                     2, /* maxParams */
                     asynInt32ArrayMask | asynInt32Mask | asynDrvUserMask, /* interface mask*/
                     asynInt32ArrayMask, /* interrupt mask */
                     0, /* asynFlags - does not block, no addresses */
                     1, /* autoconnect */
                     0, /* default priority */
                     0) /* default stack size */,
      length(length), ofs(0), delay(delay), period(period), tick(0)
{
    printf("accumulate init: %s period %d\n", portName, period);
    buffer = (int32_t *)calloc(length, sizeof(epicsInt32));
    assert(buffer);
    
    createParam("TRIGGER",  asynParamInt32, &P_Trigger);
    createParam("BUFFER",   asynParamInt32Array, &P_Buffer);

    asynUser * pasynUser = pasynManager->createAsynUser(queue_request_helper, 0);
    asynStatus status = pasynManager->connectDevice(pasynUser, ecPortName, addr);
    assert(status == asynSuccess);
    asynInterface * pasynInterface = pasynManager->findInterface(pasynUser, asynDrvUserType, 1);
    assert(pasynInterface);
    asynDrvUser * drvUser = (asynDrvUser *)pasynInterface->pinterface;
    assert(drvUser);
    drvUser->create(pasynInterface->drvPvt, pasynUser, name, NULL, NULL);

    pasynInterface = pasynManager->findInterface(pasynUser, asynInt32Type ,1);
    assert(pasynInterface);
    asynInt32 * pAsynInt32 = (asynInt32 *)pasynInterface->pinterface;

    // what does registrar do?
    void * registrar;
    pAsynInt32->registerInterruptUser(pasynInterface->drvPvt, pasynUser, 
                                      interrupt_callback_helper, this, &registrar);


    
    
    
}

asynStatus ecAccumulate::readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                        size_t nElements, size_t *nIn)
{
    if((int)nElements > length) nElements = length;
    memcpy(value, buffer, nElements * sizeof(epicsInt32));
    *nIn = nElements;
    return asynSuccess;
}

struct ecDrvUser
{
    char * drvInfo;
    int routing;
};

class ecAsynPort : public asynPortDriver 
{
public:
    ecAsynPort(const char *portName, const char * socketName, 
               int default_period,
               int selftest);
    /* overriden from asynPortDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 * value);
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize);
    asynStatus doCallback(monitor_response * m);
    void createSubscription(int addr, const char * usr, int route);
    void reader();
    void writer();
    const char * socket_name;
private:
    gphPvt * commands;
    int max_channel;
    int default_period;
    ScanMock * scanner;
    epicsThreadId reader_thread;
    epicsThreadId writer_thread;
    epicsMessageQueueId commandq;
};

MAKE_DISPATCH_HELPER(ecAsynPort, reader);
MAKE_DISPATCH_HELPER(ecAsynPort, writer);

void ecAsynPort::reader()
{
    char msg[MAX_MESSAGE];
    int * tag = (int *)msg;

    int sock = -1;

    while(1)
    {

        while(1)
        {
            printf("connection attempt\n");
            sock = rtSockCreate(socket_name);
            if(sock != -1)
            {
                printf("connected\n");
                break;
            }
            epicsThreadSleep(1.0);
        }
        
        tag[0] = MSG_CONNECT;
        tag[1] = sock;
        epicsMessageQueueSend(commandq, msg, sizeof(msg));
        
        while(1)
        {
            int sz = rtSockReceive(sock, msg, sizeof(msg));
            if(sz < 0)
            {
                printf("receive connection closed\n");
                break;
            }
            if(tag[0] == MSG_REPLY)
            {
                monitor_response * resp = (monitor_response *)msg;
                doCallback(resp);
            }
        }

        close(sock);
        
    }
    
}

struct monitor_request_node
{
    ELLNODE node;
    monitor_request req;
};

void ecAsynPort::writer()
{

    char msg[MAX_MESSAGE];
    int * tag = (int *)msg;
    int sock = 0;
    ELLLIST monitors;
    ellInit(&monitors);
    
    while(1)
    {
        int sz = epicsMessageQueueReceive(commandq, msg, sizeof(msg));
        assert(sz > 0);
        if(tag[0] == MSG_CONNECT)
        {
            printf("writer connect\n");
            sock = tag[1];
            monitor_request_node * node;
            for(node = (monitor_request_node *)ellFirst(&monitors); 
                node != NULL; 
                node = (monitor_request_node *)ellNext(&node->node))
            {
                rtSockSend(sock, &node->req, sizeof(monitor_request));
            }
        }
        else if(tag[0] == MSG_MONITOR)
        {
            monitor_request * m = (monitor_request *)msg;
            monitor_request_node * next = 
                (monitor_request_node *)calloc(1, sizeof(monitor_request_node));
            memcpy(&next->req, m, sizeof(monitor_request));
            ellAdd(&monitors, &next->node);
            if(sock != 0)
            {
                rtSockSend(sock, m, sizeof(monitor_request));
            }
        }
        else if(tag[0] == MSG_WRITE)
        {
            write_request * w = (write_request *)msg;
            if(sock != 0)
            {
                rtSockSend(sock, w, sizeof(write_request));
            }
            
        }
    }
}

asynStatus ecAsynPort::doCallback(monitor_response * m)
{
    ELLLIST *pclientList;
    interruptNode *node;
    asynStandardInterfaces *pInterfaces = &asynStdInterfaces;
    assert(pInterfaces->int32InterruptPvt);
    pasynManager->interruptStart(pInterfaces->int32InterruptPvt, &pclientList);
    for(node = (interruptNode *)ellFirst(pclientList); 
        node != NULL; 
        node = (interruptNode *)ellNext(&node->node))
    {
        int addr = 0;
        asynInt32Interrupt *pInterrupt = (asynInt32Interrupt *) node->drvPvt;
        pasynManager->getAddr(pInterrupt->pasynUser, &addr);

        ecDrvUser * user = (ecDrvUser *)pInterrupt->pasynUser->drvUser;
        if(user->routing == m->routing && addr == m->vaddr)
        {
            // TODO oversampling modules produce 10 samples in a row
            // not a waveform
            for(int s = 0; s < m->length; s++)
            {
                pInterrupt->callback(pInterrupt->userPvt, pInterrupt->pasynUser, 
                                     m->value);
            }
        }
     }
    pasynManager->interruptEnd(pInterfaces->int32InterruptPvt);
    return asynSuccess;
}

void ecAsynPort::createSubscription(int addr, const char * usr, int route)
{
    monitor_request m;
    m.tag = MSG_MONITOR;
    m.period = default_period;
    strncpy(m.usr, usr, sizeof(m.usr)-1);
    for(int n = 0; n < (int)strlen(usr); n++)
    {
        if(m.usr[n] == ',')
        {
            sscanf(m.usr + n, ",%d", &m.period);
            printf("new period is %d\n", m.period);
            m.usr[n] = '\0';
            break;
        }
    }
    m.routing = route;
    m.vaddr = addr;
    epicsMessageQueueSend(commandq, &m, sizeof(m));
}

asynStatus ecAsynPort::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize)
{
    char *functionName = "drvUserCreate";

    int addr = 0;
    pasynManager->getAddr(pasynUser, &addr);

    ecDrvUser * user = (ecDrvUser *)calloc(1, sizeof(ecDrvUser));
    user->drvInfo = strdup(drvInfo);
    user->routing = max_channel++;
    
    pasynUser->drvUser = user;
    
    // TODO drvUser create is called TWICE per record
    // but only one interrupt user is registered
    // don't create duplicate monitors
    // we are subscribing to output records too!
    
    createSubscription(addr, drvInfo, user->routing);

    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s: %p drvInfo '%s'\n", functionName, pasynUser, drvInfo);
    
    return asynSuccess;
}

asynStatus ecAsynPort::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    char *functionName = "writeInt32";
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s: writing int 32 %d %p\n", functionName, value, 
              pasynUser->drvUser);

    int addr = 0;
    pasynManager->getAddr(pasynUser, &addr);

    write_request w;
    w.tag = MSG_WRITE;
    w.vaddr = addr;
    w.value = value;
    strncpy(w.usr, ((ecDrvUser *)pasynUser->drvUser)->drvInfo, sizeof(w.usr));

    epicsMessageQueueSend(commandq, &w, sizeof(w));

    return asynSuccess;
}

asynStatus ecAsynPort::readInt32(asynUser *pasynUser, epicsInt32 * value)
{
    // TODO ok for EPICS records, the value is pushed by the interrupt
    // other asyn users may require the real data
    *value = 0;
    return asynSuccess;
}

ecAsynPort::ecAsynPort(const char *portName, const char * sockName, 
                       int default_period,
                       int selftest)
    : asynPortDriver(portName,
                     0, /* maxAddr - not using C++ params */
                     0, /* maxParams - not using C++ params*/
                     asynInt32Mask | asynDrvUserMask, /* interface mask*/
                     asynInt32Mask, /* interrupt mask */
                     ASYN_MULTIDEVICE, /* asynFlags - does not block, supports addresses */
                     1, /* autoconnect */
                     0, /* default priority */
                     0), /* default stack size */
      max_channel(0), default_period(default_period)
{
    printf("ethercat client: %s %s\n", portName, sockName);
    this->socket_name = strdup(sockName);

    commandq = epicsMessageQueueCreate(10, MAX_MESSAGE);

    if(selftest)
    {
        printf("starting built-in self test scanner mockup\n");
        scanner = new ScanMock(this->socket_name);
    }

    reader_thread = epicsThreadCreate(
        "reader", 0, epicsThreadGetStackSize(epicsThreadStackMedium), ecAsynPort_reader_start, this);
    writer_thread = epicsThreadCreate(
        "writer", 0, epicsThreadGetStackSize(epicsThreadStackMedium), ecAsynPort_writer_start, this);
}

extern "C"
{

    /* EPICS iocsh shell commands */

    static const iocshArg InitArg[] = { { "portName", iocshArgString},
                                        { "sockName", iocshArgString},
                                        { "period",   iocshArgInt},
                                        { "selftest", iocshArgInt   } };


    static const iocshArg * const InitArgs[] = { &InitArg[0], 
                                                 &InitArg[1], 
                                                 &InitArg[2],
                                                 &InitArg[3] };
    
    static const iocshFuncDef initFuncDef = {"ecConfigure", 4, InitArgs};

    static void initCallFunc(const iocshArgBuf *args)
    {
        new ecAsynPort(args[0].sval, args[1].sval, 
                       args[2].ival, args[3].ival );
    }

    static const iocshArg AccArg[] = { { "portName", iocshArgString },
                                       { "ecPort",   iocshArgString },
                                       { "address",  iocshArgInt    },
                                       { "name",     iocshArgString },
                                       { "length",   iocshArgInt    },
                                       { "delay",    iocshArgInt    }, 
                                       { "period",   iocshArgInt    } };

    static const iocshArg * const AccArgs[] = { &AccArg[0], 
                                                &AccArg[1], 
                                                &AccArg[2], 
                                                &AccArg[3], 
                                                &AccArg[4], 
                                                &AccArg[5],
                                                &AccArg[6] };
    
    static const iocshFuncDef AccFuncDef = {"accConfigure", 7, AccArgs};
    
    static void AccCallFunc(const iocshArgBuf *args)
    {
        new ecAccumulate(args[0].sval, args[1].sval, 
                         args[2].ival, args[3].sval, 
                         args[4].ival, args[5].ival, args[6].ival);
    }
    
    void ecAsynPortDriverRegister(void)
    {
        iocshRegister(&initFuncDef,initCallFunc);
        iocshRegister(&AccFuncDef, AccCallFunc);
    }
    
    epicsExportRegistrar(ecAsynPortDriverRegister);

}
