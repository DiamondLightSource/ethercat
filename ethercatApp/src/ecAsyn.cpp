
#include <string.h>

#include <stdlib.h>
#include <unistd.h>
#include <epicsThread.h>
#include <epicsExport.h>


#include <iocsh.h>
#include <cantProceed.h>

#include "classes.h"
#include "parser.h"
#include "unpack.h"
#include "rtutils.h"
#include "msgsock.h"
#include "messages.h"

#include "gadc.h"
#include "ecAsyn.h"

template <typename T> T * node_cast(ELLNODE * node)
{
    // static_cast understands the object layout
    // even in the presence of VTABLE etc.
    return static_cast<T *>((ListNode<T> *)node);
}

enum 
{ 
    EC_WC_STATE_ZERO = 0, 
    EC_WC_STATE_INCOMPLETE = 1, 
    EC_WC_STATE_COMPLETE = 2 
};

enum 
{ 
    EC_AL_STATE_INIT = 1,
    EC_AL_STATE_PREOP = 2,
    EC_AL_STATE_SAFEOP = 4,
    EC_AL_STATE_OP = 8
};

struct sampler_config_t
{
    ELLNODE node;
    char * port;
    int channel;
    char * sample;
    char * cycle;
};

static ELLLIST sampler_configs;
static int showPdosEnabled = 0;

static void showPdos(int enable)
{
    showPdosEnabled = enable;    
}
// Called by the IOC command "ADC_Ethercat_Sampler"
static void Configure_Sampler(char * port, int channel, char * sample, char * cycle)
{
    sampler_config_t * conf = (sampler_config_t *)callocMustSucceed
        (1, sizeof(sampler_config_t), "can't allocate sampler config buffer");
    if(port == NULL || sample == NULL)
    {
        printf("adc configure error: port %p sample %p\n", port, sample);
        return;
    }
    conf->port = strdup(port);
    conf->channel = channel;
    conf->sample = strdup(sample);
    if(cycle != NULL)
    {
        conf->cycle = strdup(cycle);
    }
    ellAdd(&sampler_configs, &conf->node);
}

/* used in 99.9% of programs */
static char * format(const char *fmt, ...)
{
    char * buffer = NULL;
    va_list args;
    va_start(args, fmt);
    int ret = vasprintf(&buffer, fmt, args);
    assert(ret != -1);
    va_end(args);
    return buffer;
}

static char * makeParamName(EC_PDO_ENTRY_MAPPING * mapping)
{
    char * name = format("%s.%s", mapping->pdo_entry->parent->name, mapping->pdo_entry->name);
    // remove spaces
    int out = 0;
    for(int n = 0; n < (int)strlen(name); n++)
    {
        if(name[n] != ' ')
        {
            name[out++] = name[n];
        }
    }
    name[out] = '\0';
    return name;
}

static EC_PDO_ENTRY_MAPPING * mapping_by_name(EC_DEVICE * device, const char * name)
{
    for(ELLNODE * node = ellFirst(&device->pdo_entry_mappings); node; node = ellNext(node))
    {
        EC_PDO_ENTRY_MAPPING * mapping = (EC_PDO_ENTRY_MAPPING *)node;
        char * entry_name = makeParamName(mapping);
        int match = strcmp(name, entry_name) == 0;
        free(entry_name);
        if(match)
        {
            return mapping;
        }
    }
    return NULL;
}

class Sampler : public ProcessDataObserver
{
protected:
    ecAsyn * parent;
    EC_PDO_ENTRY_MAPPING * sample;
    WaveformPort * wave;
public:
    Sampler(ecAsyn * parent, int channel, 
            EC_PDO_ENTRY_MAPPING * sample) : 
        parent(parent), sample(sample), 
        wave(new WaveformPort(format("%s_ADC%d", parent->portName, channel), parent, sample))
        { }
    virtual void on_pdo_message(PDO_MESSAGE * pdo, int size)
        {
            wave->lock();
            wave->setPutsample(cast_int32(sample, pdo->buffer, 0));
            wave->unlock();
        }
    virtual ~Sampler() {}
};

class Oversampler : public Sampler
{
    EC_PDO_ENTRY_MAPPING * cycle;
    int lastCycle;
    XFCPort * xfc;
    int missed;
    int P_Missed;

public:
    Oversampler(ecAsyn * parentPort, int channel, 
               EC_PDO_ENTRY_MAPPING * sample, EC_PDO_ENTRY_MAPPING * cycle) : 
        Sampler(parentPort, channel, sample), cycle(cycle), lastCycle(0), 
        xfc(new XFCPort(format("%s_XFC%d", parent->portName, channel))) {}
    virtual void on_pdo_message(PDO_MESSAGE * pdo, int size)
        {
            int stride = parent->device->oversampling_rate;
            int32_t cyc = cast_int32(cycle, pdo->buffer, 0);
            if(lastCycle == cyc)
            {
                // skip duplicates
                //printf("%s Duplicate %d\n", parent->portName, cyc);
                return;
            }
            if((lastCycle + 1) % 65536 != cyc)
            {
                xfc->lock();
                xfc->incMissed();
                printf("%s Missed %d\n", parent->portName, (lastCycle + 1) % 65536);
                xfc->unlock();
            }
            lastCycle = cyc;
            int16_t * samples = (int16_t *)(pdo->buffer + sample->offset);
            wave->lock();
            for(int s = 0; s < stride; s++)
            {
                wave->setPutsample(samples[s]);
            }
            wave->unlock();
        }
};

struct ENGINE_USER
{
    ecMaster * master;
    ELLLIST ports;
    EC_CONFIG * config;
    rtMessageQueueId config_ready;
    int count;
    rtMessageQueueId writeq;
    void * config_buffer;
    int config_size;
};

ecMaster::ecMaster(char * name) :
    asynPortDriver(name,
                   1, /* maxAddr */
                   NUM_MASTER_PARAMS, /* max parameters */
                   asynInt32Mask | asynDrvUserMask, /* interface mask*/
                   asynInt32Mask, /* interrupt mask */
                   0, /* non-blocking, no addresses */
                   1, /* autoconnect */
                   0, /* default priority */
                   0) /* default stack size */,
    lastCycle(0), missed(0)
{
    printf("create master %s\n", name);
    createParam("Cycle", asynParamInt32, &P_Cycle);
    createParam("WorkingCounter", asynParamInt32, &P_WorkingCounter);
    createParam("Missed", asynParamInt32, &P_Missed);
    createParam("WcState", asynParamInt32, &P_WcState);
}

/**
 *  Creation of asyn port for an ethercat slave
 */
ecAsyn::ecAsyn(EC_DEVICE * device, int pdos, ENGINE_USER * usr, int devid) :
    asynPortDriver(device->name,
                   1, /* maxAddr */
                   NUM_SLAVE_PARAMS + pdos, /* max parameters */
                   asynOctetMask | asynInt32Mask | asynInt32ArrayMask 
                   | asynDrvUserMask, /* interface mask*/
                   asynOctetMask| asynInt32Mask | asynInt32ArrayMask, /* interrupt mask */
                   0, /* asyn flags: non-blocking, no addresses */
                   1, /* autoconnect */
                   0, /* default priority */
                   0) /* default stack size */,
    pdos(pdos), 
    devid(devid), 
    writeq(usr->writeq), 
    mappings(new EC_PDO_ENTRY_MAPPING * [pdos]),
    device(device)
{
    printf("ecAsyn INIT type %s name %s PDOS %d\n", device->type_name, device->name, pdos);

    for(ELLNODE * node = ellFirst(&sampler_configs); node; node = ellNext(node))
    {
        sampler_config_t * conf = (sampler_config_t *)node;
        if(strcmp(conf->port, device->name) == 0)
        {
            printf("sampler ports for device %s\n", device->name);
            Sampler * s = NULL;
            EC_PDO_ENTRY_MAPPING * sample_mapping = mapping_by_name(device, conf->sample);
            if(sample_mapping != NULL)
            {
                if(conf->cycle == NULL)
                {
                    s = new Sampler(this, conf->channel, sample_mapping);
                }
                else
                {
                    EC_PDO_ENTRY_MAPPING * cycle_mapping = mapping_by_name(device, conf->cycle);
                    if(cycle_mapping != NULL)
                    {
                        s = new Oversampler(this, conf->channel, sample_mapping, cycle_mapping);
                    }
                }
            }
            if(s != NULL)
            {
                ellAdd(&usr->ports, &s->node);
            }
        }
    }
    
    int n = 0;
    P_First_PDO = -1;
    for(ELLNODE * node = ellFirst(&device->pdo_entry_mappings); node; node = ellNext(node))
    {
        assert(n < pdos);
        EC_PDO_ENTRY_MAPPING * mapping = (EC_PDO_ENTRY_MAPPING *)node;
        char * name = makeParamName(mapping);
        printf("createParam %s\n", name);
        int param;
        assert(createParam(name, asynParamInt32, &param) == asynSuccess);
        if(P_First_PDO == -1)
        {
            P_First_PDO = param;
        }
        P_Last_PDO = param;
        mapping->pdo_entry->parameter = param;
        mappings[n] = mapping;
        n++;
    }

    if ( (strcmp(this->device->type_name, "EL3602") == 0)
         && (this->device->type_revid == 0x00120000) )
    {
        for( n = 0; n < pdos; n++)
        {
            // The ADC values for Beckhoff EL3602 are encoded in the
            // top 3 bytes of the 32 bit word
            if ( mappings[n]->pdo_entry->bits == 32 )
                mappings[n]->shift = 8;
        }
    }

    int status = asynSuccess;
    status |= createParam(ECALStateString,   asynParamInt32, &P_AL_STATE);
    status |= createParam(ECErrorFlagString, asynParamInt32, &P_ERROR_FLAG);
    status |= createParam(ECDisableString,   asynParamInt32, &P_DISABLE);
    status |= createParam(ECDeviceTypename,  asynParamOctet, &P_DEVTYPENAME);
    status |= createParam(ECDeviceRevision,  asynParamOctet, &P_DEVREVISION);
    status |= createParam(ECDevicePosition,  asynParamOctet, &P_DEVPOSITION);
    status |= createParam(ECDeviceName,      asynParamOctet, &P_DEVNAME);
    assert(status == asynSuccess);
    setIntegerParam(P_DISABLE, 1);

    status = asynSuccess;
    char *rev_string = format("Rev 0x%x", device->type_revid);
    char *posbuffer = format("Pos %d", device->position);
    status |= setStringParam(P_DEVNAME, device->name);
    status |= setStringParam(P_DEVPOSITION, posbuffer);
    status |= setStringParam(P_DEVREVISION, rev_string);
    status |= setStringParam(P_DEVTYPENAME, device->type_name);
    free(posbuffer);
    free(rev_string);
    assert(status == asynSuccess);

}

void ecMaster::on_pdo_message(PDO_MESSAGE * pdo, int size)
{
    lock();
    if(pdo->cycle != lastCycle + 1)
    {
        missed++;
    }
    lastCycle = pdo->cycle;
    setIntegerParam(P_Cycle, pdo->cycle);
    setIntegerParam(P_WorkingCounter, pdo->working_counter);
    setIntegerParam(P_WcState, pdo->wc_state);
    setIntegerParam(P_Missed, missed & INT32_MAX);
    callParamCallbacks();
    unlock();
}

asynStatus ecAsyn::getBoundsForMapping(EC_PDO_ENTRY_MAPPING * mapping, epicsInt32 *low, epicsInt32 *high)
{
    asynStatus result = asynSuccess;
    switch (mapping->pdo_entry->bits)
    {
    case 24:
        *low = -8388608;
        *high = 8388607;
        break;
    case 16:
        *low = -32768;
        *high = 32767;
        break;
    case 32:
        if (mapping->shift == 8)
        {
            *low = -8388608;
            *high = 8388607;
        }
        else
        {
            *low = INT32_MIN;
            *high = INT32_MAX;
        }
        break;
    default:
        result = asynError;
        *low = 0;
        *high = 0;
    }
    return result;
}


asynStatus ecAsyn::getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high)
{
    static const char *driverName = "ecAsyn";
    int cmd = pasynUser->reason;
    if (cmd >= P_First_PDO && cmd <= P_Last_PDO)
    {
        int pdo = cmd - P_First_PDO;
        assert(pdo >= 0 && pdo < pdos);
        EC_PDO_ENTRY_MAPPING * mapping = mappings[pdo];

        this->getBoundsForMapping(mapping, low, high);

        if ( ( strcmp(this->device->type_name, "EL3602") == 0 ) 
             && this->device->type_revid == 0x00120000 )
        {
            mapping->shift = 8;
        }
    }
    else
    {
        *low = 0;
        *high = 0;
    }
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s::getBounds,low=%d, high=%d\n", 
              driverName, *low, *high);
    return(asynSuccess);  
}

void ecAsyn::on_pdo_message(PDO_MESSAGE * pdo, int size)
{
    lock();
    char * meta = pdo->buffer + pdo->size + 2 * devid;
    assert(meta + 1 - pdo->buffer < size);
    epicsInt32 al_state = meta[0];
    epicsInt32 error_flag = meta[1];
    epicsInt32 disable = pdo->wc_state == EC_WC_STATE_ZERO || al_state != EC_AL_STATE_OP;
    epicsInt32 lastDisable;
    assert(getIntegerParam(P_DISABLE, &lastDisable) == asynSuccess); // can't fail
    setIntegerParam(P_AL_STATE, al_state);
    setIntegerParam(P_ERROR_FLAG, error_flag);
    setIntegerParam(P_DISABLE, disable);

    for(ELLNODE * node = ellFirst(&device->pdo_entry_mappings); node; node = ellNext(node))
    {
        EC_PDO_ENTRY_MAPPING * mapping = (EC_PDO_ENTRY_MAPPING *)node;
        int32_t val = cast_int32(mapping, pdo->buffer, 0);
        // can't make SDIS work with I/O intr (some values get lost)
        // so using this for now
        if(disable)
        {
            setIntegerParam(mapping->pdo_entry->parameter, INT32_MIN);
        }
        else
        {
            setIntegerParam(mapping->pdo_entry->parameter, val);
        }
    }
    callParamCallbacks();
    unlock();
}

asynStatus ecAsyn::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    asynStatus status = asynPortDriver::writeInt32(pasynUser, value);
    int cmd = pasynUser->reason;
    /* printf("writing %d -> %d, first %d last %d\n", cmd, value, P_First_PDO, P_Last_PDO); */
    if(cmd >= P_First_PDO && cmd <= P_Last_PDO)
    {
        int pdo = cmd - P_First_PDO;
        assert(pdo >= 0 && pdo < pdos);
        EC_PDO_ENTRY_MAPPING * mapping = mappings[pdo];
        /* printf("pdo %d mapping %p\n", pdo, mapping); */
        WRITE_MESSAGE write;
        write.tag = MSG_WRITE;
        write.offset = mapping->offset;
        write.bit_position = mapping->bit_position;
        write.bits = mapping->pdo_entry->bits;
        write.value = value;
        rtMessageQueueSend(writeq, &write, sizeof(WRITE_MESSAGE));
        return asynSuccess;
    }
    return status;
}

/*
 *  read configuration sent by scanner and populate "config" in 
 *  ENGINE_USER structure
 */
static int init_unpack(ENGINE_USER * usr, char * buffer, int size)
{
    EC_CONFIG * cfg = usr->config;
    int ofs = 0;
    int tag = unpack_int(buffer, &ofs);
    assert(tag == MSG_CONFIG);
    int scanner_config_size = unpack_int(buffer, &ofs);
    read_config(buffer + ofs, scanner_config_size, cfg);
    ofs += scanner_config_size;
    int mapping_config_size = unpack_int(buffer, &ofs);
    parseEntriesFromBuffer(buffer + ofs, mapping_config_size, cfg);

    ELLNODE * node;
    for(node = ellFirst(&cfg->devices); node; node = ellNext(node))
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        printf("%s\n", device->name);
        ELLNODE * node1;
        for(node1 = ellFirst(&device->pdo_entry_mappings); node1; node1 = ellNext(node1))
        {
            EC_PDO_ENTRY_MAPPING * mapping = (EC_PDO_ENTRY_MAPPING *)node1;
            if(strcmp(mapping->pdo_entry->name, mapping->pdo_entry->parent->name) == 0)
            {
                printf("  %s\n", mapping->pdo_entry->name);
            }
            else
            {
                printf("  %s %s\n", mapping->pdo_entry->parent->name, mapping->pdo_entry->name);
            }
        }
    }
    return 0;
}

static void readConfig(ENGINE_USER * usr)
{
    EC_CONFIG * cfg = usr->config;
    ELLNODE * node;
    int ndev = 0;
    for(node = ellFirst(&cfg->devices); node; node = ellNext(node))
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        printf("Creating ecAsyn port No %d: %s\n", ndev, device->name);
        ecAsyn * port = new ecAsyn(device, 
                                   device->pdo_entry_mappings.count, 
                                   usr, ndev);
        ellAdd(&usr->ports, &port->node);
        ndev++;
    }
}

static int receive_config_on_connect(ENGINE * engine, int sock)
{
    printf("getting config\n");
    ENGINE_USER * usr = (ENGINE_USER *)engine->usr;
    int ack = 0;
    int size = rtSockReceive(sock, engine->receive_buffer, engine->max_message);
    if(size > 0)
    {
        if(usr->config_buffer == NULL)
        {
            usr->config_size = size;
            usr->config_buffer = callocMustSucceed
                (size, sizeof(char), "can't allocate config XML receive buffer");
            memcpy(usr->config_buffer, engine->receive_buffer, size);
            printf("config-file size:%d\n", size);
            printf("%s\n", (char *) usr->config_buffer);
            printf("************************\n");
            
            init_unpack(usr, engine->receive_buffer, size);
            readConfig(usr);
            rtMessageQueueSend(usr->config_ready, &ack, sizeof(int));
        }
        else
        {
            // check that the config hasn't changed
            assert(size == usr->config_size && 
                   memcmp(usr->config_buffer, engine->receive_buffer, size) == 0);
        }
    }
    return !(size > 0);
}

int show_pdo_data(char * buffer, int size, EC_CONFIG *cfg)
{
    EC_MESSAGE * msg = (EC_MESSAGE *)buffer;
    assert(msg->tag == MSG_PDO);
    int n;
    for(n = 0; n < size; n++)
    {
        printf("%02x ", (unsigned char)buffer[n]);
        if((n % 8) == 15)
        {
            printf("\n");
        }
    }
    printf("\n");
    printf("bys cycle %d working counter %d state %d\n", msg->pdo.cycle, msg->pdo.working_counter, 
           msg->pdo.wc_state);
    ELLNODE * node;
    for(node = ellFirst(&cfg->devices); node; node = ellNext(node))
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        printf("%s\n", device->name);
        ELLNODE * node1;
        for(node1 = ellFirst(&device->pdo_entry_mappings); node1; node1 = ellNext(node1))
        {
            EC_PDO_ENTRY_MAPPING * mapping = (EC_PDO_ENTRY_MAPPING *)node1;

            printf(" %s ", mapping->pdo_entry->parent->name);
            printf( "%s ", mapping->pdo_entry->datatype);

            if(mapping->pdo_entry->oversampling)
            {
                for(n = 0; n < mapping->parent->oversampling_rate; n++)
                {
                    printf("%d ", cast_int32(mapping, msg->pdo.buffer, n));
                }
                printf("\n");
            }
            else
            {
                int32_t val = cast_int32(mapping, msg->pdo.buffer, 0);
                printf("%d\n", val);
            }

        }
    }
    printf("\n");

    // store these in the ASYN int32 attributes for this device

    return 0;
}

static int pdo_data(ENGINE_USER * usr, char * buffer, int size)
{
    EC_MESSAGE * msg = (EC_MESSAGE *)buffer;
    assert(msg->tag == MSG_PDO);
    static int c = 0;
    //#endif
    for(ELLNODE * node = ellFirst(&usr->ports); node; node = ellNext(node))
    {
        node_cast<ProcessDataObserver>(node)->on_pdo_message(&msg->pdo, size);
    }
    if (showPdosEnabled)
    {
        if ( c % 1000 == 0 )
        {
            c = 0;
            show_pdo_data(buffer, size, usr->config);
        }
        c++;
    }
    return 0;
}

static int ioc_send(ENGINE * server, int size)
{
    ENGINE_USER * usr = (ENGINE_USER *)server->usr;
    pdo_data(usr, server->receive_buffer, size);
    return 0;
}

static int ioc_receive(ENGINE * server)
{
    ENGINE_USER * usr = (ENGINE_USER *)server->usr;
    int size = rtMessageQueueReceive(usr->writeq, server->send_buffer, server->max_message);
    return size;
}

/*
 * Driver initialisation at IOC startup (ecAsynInit)
 *
 * path - location of Unix Domain Socket, must match the scanner's
 * max_message - maximum size of messages between scanner and ioc
 *               This must be able to accommodate the configuration
 *               of the chain that is transferred from the scanner to 
 *               the ioc.
 */
static void makePorts(char * path, int max_message)
{
    ENGINE_USER * usr = (ENGINE_USER *)callocMustSucceed
        (1, sizeof(ENGINE_USER), "can't allocate socket engine private data");
    ellInit(&usr->ports);
    usr->master = new ecMaster((char *)"MASTER0");
    ellAdd(&usr->ports, &usr->master->node);
    usr->config_ready = rtMessageQueueCreate(1, sizeof(int));
    // TODO - no assert for runtime errors, so what should we use to throw?
    assert(usr->config_ready != NULL);
    usr->config = (EC_CONFIG *)callocMustSucceed
        (1, sizeof(EC_CONFIG), "can't allocate chain config lists");
    usr->writeq = rtMessageQueueCreate(1, max_message);
    assert(usr->writeq != NULL);
    ENGINE * engine = new_engine(max_message);
    engine->path = strdup(path);
    engine->connect = client_connect;
    engine->on_connect = receive_config_on_connect;
    engine->send_message = ioc_send;
    engine->receive_message = ioc_receive;
    engine->usr = usr;
    engine_start(engine);

    new_timer(1000000000, usr->writeq, 0, MSG_HEARTBEAT);

    int ack;
    rtMessageQueueReceive(usr->config_ready, &ack, sizeof(int));

}

extern "C"
{
    /* EPICS iocsh shell commands */
    static const iocshArg InitArg[] = { { "path",       iocshArgString },
                                        { "maxmessage", iocshArgInt    } };
    static const iocshArg * const InitArgs[] = { &InitArg[0], &InitArg[1] };
    static const iocshFuncDef InitFuncDef = {"ecAsynInit", 2, InitArgs};
    static void InitCallFunc(const iocshArgBuf * args)
    {
        makePorts(args[0].sval, args[1].ival);
    }

    static const iocshArg SamplerArg[] = { { "port",       iocshArgString },
                                           { "channel",    iocshArgInt    },
                                           { "sample",     iocshArgString },
                                           { "cycle",      iocshArgString }};

    static const iocshArg * const SamplerArgs[] = { &SamplerArg[0], 
                                                    &SamplerArg[1], 
                                                    &SamplerArg[2], 
                                                    &SamplerArg[3] };
    
    static const iocshFuncDef SamplerFuncDef = {"ADC_Ethercat_Sampler", 4, SamplerArgs};
    static void SamplerCallFunc(const iocshArgBuf * args)
    {
        Configure_Sampler(args[0].sval, args[1].ival, args[2].sval, args[3].sval);
    }

    static const iocshArg showPdoArg[] = { {"enable", iocshArgInt}};
    static const iocshArg * const showPdoArgs[] = { &showPdoArg[0] };
    static const iocshFuncDef showPdoFuncDef = { "showPdo", 1, showPdoArgs};
    static void showPdoCallFunc(const iocshArgBuf * args)
    {
        showPdos(args[0].ival);
    }
    void ecAsynRegistrar(void)
    {
        ellInit(&sampler_configs);
        iocshRegister(&InitFuncDef,InitCallFunc);
        iocshRegister(&SamplerFuncDef,SamplerCallFunc);
        iocshRegister(&showPdoFuncDef,showPdoCallFunc);
    }
    epicsExportRegistrar(ecAsynRegistrar);
}


/// XFCPort constructor
XFCPort::XFCPort(const char * name) : asynPortDriver(
        name,
        1, /* maxAddr */
        1, /* max parameters */
        asynInt32Mask | asynDrvUserMask, /* interface mask*/
        asynInt32Mask, /* interrupt mask */
        0, /* non-blocking, no addresses */
        1, /* autoconnect */
        0, /* default priority */
        0) /* default stack size */
{
    printf("Creating XFCPort \"%s\"\n", name);
    createParam("MISSED", asynParamInt32, &P_Missed);
    setIntegerParam(P_Missed, 0);
}


