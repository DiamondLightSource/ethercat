#include <sys/socket.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>

#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <epicsExport.h>
#include <iocsh.h>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include "asynPortDriver.h"
#include "commands.h"
#include "buffer.h"


char * socket_name = "/tmp/scanner.sock";

/* max virtual channels */
#define MAX_CHANNELS 256

struct worker_data_t 
{
    int fd;         /* socket */
    buffer_t queue; /* write_queue or read_queue */
    int connection_ok;   /* signal end of loop */
    epicsThreadId thread; /* worker thread id */
};
typedef struct worker_data_t worker_data_t;

int enable_writer_msg = 1;
int enable_reader_msg = 1;

/** Thread that takes commands from writer_data.queue and writes to the socket 
 */
void writer_start(void * usr)
{
    char *functionName = "writer_start";
    worker_data_t *writerData = (worker_data_t *) usr;
    ssize_t write_res;
    cmd_packet packet = {0};
    while(writerData->connection_ok)
    {
        buffer_get(&(writerData->queue), &packet);
        if (enable_writer_msg)
            printf("%s: writing cmd %s chan %d\n", functionName, 
                    command_strings[packet.cmd], packet.channel);
        if (packet.cmd == DISCONN) {
            writerData->connection_ok = 0;
            printf("Disconnection requested for writer thread\n");
        }
        else {
            write_res = write(writerData->fd, &packet, sizeof(packet));
            if (write_res != sizeof(packet)) 
            {
                printf("Write failed in writer thread");
                printf("%s %d. write_res is %d\n", __FILE__, __LINE__, write_res);
                writerData->connection_ok = 0;
            }
        }
    }
    printf("Ending writer thread.\n");
}

/** Thread that takes messages from the socket and places them on reader_data.queue
 */
void reader_start(void * usr)
{
    char *functionName = "reader_start";
    worker_data_t *readerData = (worker_data_t *) usr;
    ssize_t read_result;
    cmd_packet packet = {0};
    cmd_packet disconnection = { DISCONN };
    while(readerData->connection_ok)
    {
        read_result = read(readerData->fd, &packet, sizeof(packet));
        if (read_result != sizeof(packet)) 
        {
            printf("%s %d. read_result is %d\n", __FILE__, __LINE__, read_result);
            readerData->connection_ok = 0;
            buffer_put(&(readerData->queue), &disconnection);
            break;
        }
        buffer_put(&(readerData->queue), &packet);
        if (enable_reader_msg)
               printf("%s:result is %s ch %d val=%u\n", functionName, 
                    command_strings[packet.cmd], packet.channel, (unsigned)packet.value);
    }
}

/* ****************************************************
   EtherCAT driver ecAsynPort
 * ****************************************************
 */
class ecAsynPort : public asynPortDriver {
public:
    ecAsynPort(const char *portName, int startChannel, int endChannel);
    /* overriden from asynPortDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 * value);
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize);
    void read_channels(void);
    int  start_connection(void);
    void register_monitors(void);
    asynStatus doCallback(uint16_t channel, uint64_t value );
private:
    int socket_fd;  /*< socket to scanner */
    int start_channel;
    int n_channels;
    uint64_t *channel_data; 
    uint64_t *old_channel_data; 
    pthread_mutex_t channel_data_mutex;
    pthread_mutexattr_t channel_data_mutexattr;

    /* thread to read channels */
    epicsThreadId background_thread;
    
    worker_data_t reader_data;
    worker_data_t writer_data;
};

#define NUM_DRIVER_PARAMS 256 

char *driverName="ecAsynPort";

void *read_channels(void *drvPvt)
{
    ecAsynPort *pPvt = (ecAsynPort *)drvPvt;
    pPvt->read_channels();
    return NULL;
}

void ecAsynPort::register_monitors(void)
{
    int ch;

    cmd_packet packet;
    
    /*  read command packets */
    packet.cmd = MONITOR;
    ch = 0;
    for (ch = 0; ch < n_channels; ch++)
    {
        packet.channel = start_channel + ch;
        buffer_put(&writer_data.queue, &packet);
    }
}

asynStatus ecAsynPort::doCallback(uint16_t channel, uint64_t value )
{
    ELLLIST *pclientList;
    interruptNode *pnode;
    asynStandardInterfaces *pInterfaces = &this->asynStdInterfaces;
    int address;
    int ch;

    /* copy to channel data buffer */
    ch = channel - start_channel;
    assert( ch >= 0 && ch < n_channels);
    pthread_mutex_lock(&channel_data_mutex);
    channel_data[ch] = value;
    pthread_mutex_unlock(&channel_data_mutex);

    if ( old_channel_data[ch] == channel_data[ch] ) 
    {
        return (asynSuccess);
    }
    old_channel_data[ch] = channel_data[ch];
    /* Pass int32 interrupts */
    assert( pInterfaces->int32InterruptPvt );
    if (!pInterfaces->int32InterruptPvt) 
        return(asynParamNotFound);
    pasynManager->interruptStart(pInterfaces->int32InterruptPvt, &pclientList);
    pnode = (interruptNode *)ellFirst(pclientList);
    while (pnode) {
        asynInt32Interrupt *pInterrupt = (asynInt32Interrupt *) pnode->drvPvt;
        pasynManager->getAddr(pInterrupt->pasynUser, &address);
        /* If this is not a multi-device then address is -1, change to 0 */
        if (address == -1) address = 0;
        if (channel == pInterrupt->pasynUser->reason) {
            pInterrupt->callback(pInterrupt->userPvt,
                                 pInterrupt->pasynUser,
                                 (epicsInt32) value);
        }
        pnode = (interruptNode *)ellNext(&pnode->node);
    }
    pasynManager->interruptEnd(pInterfaces->int32InterruptPvt);
    return(asynSuccess);
}

#define CONNECTED_OK 1
#define CONNECT_FAILED 0
int ecAsynPort::start_connection()
{
    char *functionName = "ecAsynPort::start_connection";
    struct sockaddr_un address;
    size_t address_length;
    socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror(functionName);
        return CONNECT_FAILED;
    }
    address.sun_family = AF_UNIX;
    address_length = sizeof(address.sun_family) +
            sprintf(address.sun_path, "%s", socket_name);
    
    if( ::connect(socket_fd, (struct sockaddr *) &address,
                address_length) == 0)
    {
        printf("connected\n");
        
        reader_data.fd = socket_fd;
        reader_data.connection_ok = 1;
        reader_data.thread = epicsThreadCreate("ecSocketReader", 
                                 epicsThreadPriorityMedium,
                                 epicsThreadGetStackSize(epicsThreadStackMedium),
                                 (EPICSTHREADFUNC) reader_start, (void *)&reader_data);
        assert(reader_data.thread != NULL);
        writer_data.fd = socket_fd;
        writer_data.connection_ok = 1;
        writer_data.thread = epicsThreadCreate("ecSocketWriter",
                                 epicsThreadPriorityMedium,
                                 epicsThreadGetStackSize(epicsThreadStackMedium),
                                 (EPICSTHREADFUNC) writer_start, (void *)&writer_data);
        assert(writer_data.thread != NULL);
        return CONNECTED_OK;
    }
    return CONNECT_FAILED;
}

/** background channel read task that runs separately, polling
    the ethercat scanner for data from the channels */
void ecAsynPort::read_channels(void)
{
    char *functionName = "read_channels";
    cmd_packet packet;
    asynStatus callbackStatus;
    cmd_packet disconnection = { DISCONN }; 

    buffer_init(&reader_data.queue, sizeof(cmd_packet), 1000);
    buffer_init(&writer_data.queue, sizeof(cmd_packet), 1000);
    
    reader_data.connection_ok = 0;
    writer_data.connection_ok = 0;    

    while(1)
    {
        while ( ! (reader_data.connection_ok && writer_data.connection_ok) ) 
        {
            if ( start_connection() == CONNECTED_OK ) 
            {
                register_monitors();
            }
            else {
                /* wait for retry*/
                epicsThreadSleep(1.0);
            }
        }
        buffer_get(&reader_data.queue, &packet);
        
        if (enable_reader_msg)
        {
        printf("%s: command is %d %s\n", functionName, packet.cmd, 
                                          command_strings[packet.cmd]);
        printf("%s: read chan %d ch %d\n", functionName, packet.channel, 
                                       packet.channel- start_channel);
        }
        switch (packet.cmd) 
        {
        case MONITOR:
            callbackStatus = doCallback(packet.channel, packet.value);
            if (callbackStatus != asynSuccess) {
                printf("%s: failed callback for channel %d\n", functionName, 
                        packet.channel);
            }
            assert( callbackStatus == asynSuccess );
            break;
        case DISCONN:
            /* should have been switched off ! */
            if (reader_data.connection_ok) {
                /* message to reader queue to disconnect */
                assert(0); /* unexpected state, DISCONN comes from the reader_thread! */
            }
            if (writer_data.connection_ok) {
                /* message to writer queue to disconnect */
                buffer_put(&(writer_data.queue), &disconnection);
            }
            break;
        }
    }
}

/** Called by asynManager to pass a pasynUser structure and drvInfo string to the 
  * driver; assigns pasynUser->reason to one of the testParams enum value based 
  * on the value of the drvInfo string.
  * \param[in] pasynUser pasynUser structure that driver modifies
  * \param[in] drvInfo String containing information about what driver function is being referenced
  * \param[out] pptypeName Location in which driver puts a copy of drvInfo.
  * \param[out] psize Location where driver puts size of param.
  * \return Returns asynSuccess if a matching string was found, asynError if not found. */
asynStatus ecAsynPort::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                const char **pptypeName, size_t *psize)
{
    char *functionName = "drvUserCreate";
    sscanf(drvInfo, "%x", &pasynUser->reason);
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
        "%s: drvInfo %s %08x\n", functionName, drvInfo, pasynUser->reason);
    assert(pasynUser->reason >= start_channel);
    assert(pasynUser->reason <= start_channel + n_channels);

    return asynSuccess;
}


asynStatus ecAsynPort::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    char *functionName = "writeInt32";
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
        "%s: writing int 32 %d %x\n", functionName, value, pasynUser->reason);
    cmd_packet packet = {0};
    packet.cmd = WRITE;
    packet.channel = pasynUser->reason;
    packet.value    = value;
    buffer_put(&writer_data.queue, &packet);
    return asynSuccess;
}

asynStatus ecAsynPort::readInt32(asynUser *pasynUser, epicsInt32 * value)
{
    char *functionName = "readInt32";
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:reading int 32 %08x\n", functionName, pasynUser->reason);
    assert(pasynUser->reason <= start_channel + n_channels );
    int n = pasynUser->reason - start_channel;
    pthread_mutex_lock(&channel_data_mutex);
    *value = (epicsInt32) channel_data[n];
    pthread_mutex_unlock(&channel_data_mutex);
    return asynSuccess;
}

/* **********************
   Constructor
 * **********************
*/
ecAsynPort::ecAsynPort(const char *portName, int startChannel, int endChannel)
    : asynPortDriver(   portName,
                        1, /* maxAddr */
                        NUM_DRIVER_PARAMS,
                        asynInt32Mask | asynDrvUserMask, /* interface mask*/
                        asynInt32Mask, /* interrupt mask */
                        0, /* asynFlags - does not block and it's not multidevice*/
                        1, /* autoconnect */
                        0, /* default priority */
                        0) /* default stack size */
{
    assert(endChannel >= startChannel);
    start_channel = startChannel;
    n_channels = endChannel - startChannel + 1;
    assert(n_channels <= MAX_CHANNELS);

    this->channel_data = (uint64_t *) calloc(n_channels, sizeof(uint64_t));
    this->old_channel_data = (uint64_t *) calloc(n_channels, sizeof(uint64_t));
    assert(pthread_mutexattr_init(&channel_data_mutexattr) == 0);
    assert(pthread_mutex_init(&channel_data_mutex, 
                              &channel_data_mutexattr) == 0);

    
    /* create thread that reads channels in the background*/
    background_thread = epicsThreadCreate("ecAsynPortTask",
                           epicsThreadPriorityMedium,
                           epicsThreadGetStackSize(epicsThreadStackMedium),
                           (EPICSTHREADFUNC)::read_channels, this);
    assert( background_thread != NULL);
}

extern "C" {

/** EPICS iocsh callable function to call constructor for the ecAsynPortDriver class.
  * \param[in] portName     The name of the asyn port driver to be created.
  * \param[in] startChannel The first channel to read from ethercat scanner
  * \param[in] endChannel   The last channel to read from ethercat scanner */
int ecConfigure(const char *portName, int startChannel, int endChannel)
{
    new ecAsynPort(portName, startChannel, endChannel);
    return(asynSuccess);
}

void enableReaderMsg(int enable)
{
    enable_reader_msg = enable;
}

void enableWriterMsg(int enable)
{
    enable_writer_msg = enable;
}

/* EPICS iocsh shell commands */

static const iocshArg portnameArg0 = { "portName",iocshArgString};
static const iocshArg startChannelArg1 = { "startChannel",iocshArgInt};
static const iocshArg endChannelArg2 = { "endChannel", iocshArgInt};
static const iocshArg * const initArgs[] = {&portnameArg0, 
                &startChannelArg1, &endChannelArg2 };
static const iocshFuncDef initFuncDef = {"ecConfigure",3,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
    ecConfigure(args[0].sval, args[1].ival, args[2].ival);
}


static const iocshArg enableReaderArg0 = {"enable", iocshArgInt};
static const iocshArg * const enableReaderMsgArgs[] = {&enableReaderArg0};
static const iocshFuncDef enableReaderMsgFuncDef = {"enableReaderMsg", 1, 
                                                     enableReaderMsgArgs};
static void enableReaderMsgCallFunc(const iocshArgBuf *args)
{
    enableReaderMsg(args[0].ival);
}

static const iocshArg enableWriterArg0 = {"enable", iocshArgInt};
static const iocshArg * const enableWriterMsgArgs[] = {&enableWriterArg0};
static const iocshFuncDef enableWriterMsgFuncDef = {"enableWriterMsg", 1, 
                                                     enableWriterMsgArgs};
static void enableWriterMsgCallFunc(const iocshArgBuf *args)
{
    enableWriterMsg(args[0].ival);
}

void ecAsynPortDriverRegister(void)
{
    iocshRegister(&initFuncDef,initCallFunc);
    iocshRegister(&enableWriterMsgFuncDef, enableWriterMsgCallFunc);
    iocshRegister(&enableReaderMsgFuncDef, enableReaderMsgCallFunc);
}

epicsExportRegistrar(ecAsynPortDriverRegister);

}

/* TODO:  
          count of samples read
*/
