#define __STDC_LIMIT_MACROS
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <epicsThread.h>
#include <epicsExport.h>
#include <asynPortDriver.h>
#include <ellLib.h>
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

template <typename T> T * CastNode(ELLNODE * node)
{
    // check the cast from ELLNODE* to ListNode<T>*
    assert(offsetof(ListNode<T>, node) == 0);
    // static_cast understands the object layout
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
    for(NODE * node = listFirst(&device->pdo_entry_mappings); node; node = node->next)
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

class Sampler : public ListNode<Sampler>, public ProcessDataObserver
{
protected:
    ecAsyn * parent;
    EC_PDO_ENTRY_MAPPING * sample;
public:
    gadc_t * adc;
    Sampler(ecAsyn * parent, int channel, 
               EC_PDO_ENTRY_MAPPING * sample) : 
        parent(parent), sample(sample),
        adc(gadc_new(parent, channel)) 
        {
            for(int n = adc->P_First; n <= adc->P_Last; n++)
            {
                printf("reason %d write func %p\n", n, gadc_get_write_function(adc, n));
                if(gadc_get_write_function(adc, n) != NULL)
                {
                    parent->write_delegates[n] = new GADCWriteObserver(adc);
                }
            }
        }
    virtual void on_pdo_message(PDO_MESSAGE * pdo, int size)
        {
            parent->lock();
            gadc_put_sample(adc, cast_int32(sample, pdo->buffer, 0));
            parent->unlock();
        }
    virtual ~Sampler() {}
};

class Oversampler : public Sampler
{
    EC_PDO_ENTRY_MAPPING * cycle;
    int lastCycle;
    int missed;
    int P_Missed;
public:
    Oversampler(ecAsyn * parent, int channel, 
               EC_PDO_ENTRY_MAPPING * sample, EC_PDO_ENTRY_MAPPING * cycle) : 
        Sampler(parent, channel, sample), cycle(cycle), lastCycle(0), missed(0) 
        {
            parent->createParam(format("XFC%d_MISSED", channel),
                                asynParamInt32, &P_Missed);
        }
    virtual void on_pdo_message(PDO_MESSAGE * pdo, int size)
        {
            int stride = parent->device->oversampling_rate;
            int32_t cyc = cast_int32(cycle, pdo->buffer, 0);
            if(lastCycle == cyc)
            {
                // skip duplicates
                return;
            }
            parent->lock();
            if((lastCycle + 1) % 65536 != cyc)
            {
                parent->setIntegerParam(P_Missed, missed++ & INT32_MAX);
            }
            lastCycle = cyc;
            int16_t * samples = (int16_t *)(pdo->buffer + sample->offset);
            for(int s = 0; s < stride; s++)
            {
                gadc_put_sample(adc, samples[s]);
            }
            parent->unlock();
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

static const int N_RESERVED_PARAMS = 10;

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

ecAsyn::ecAsyn(EC_DEVICE * device, int pdos, rtMessageQueueId writeq, int devid) :
    asynPortDriver(device->name,
                   1, /* maxAddr */
                   pdos + N_RESERVED_PARAMS + gadc_get_num_parameters() * 8, /* max parameters */
                   asynInt32Mask | asynInt32ArrayMask | asynDrvUserMask, /* interface mask*/
                   asynInt32Mask | asynInt32ArrayMask, /* interrupt mask */
                   0, /* non-blocking, no addresses */
                   1, /* autoconnect */
                   0, /* default priority */
                   0) /* default stack size */,
    pdos(pdos), devid(devid), writeq(writeq), 
    write_delegates(new WriteObserver * [pdos + N_RESERVED_PARAMS + gadc_get_num_parameters() * 8]),
    device(device)
    
{
    ellInit(&samplers);
    ellInit(&pdo_delegates);

    printf("ecAsyn INIT %s PDOS %d\n", device->name, pdos);
    int * PdoParam = new int[pdos]; /* leak */
    int n = 0;
    
    printf("device type %s\n", device->type_name);

    for(ELLNODE * node = ellFirst(&sampler_configs); node; node = ellNext(node))
    {
        sampler_config_t * conf = (sampler_config_t *)node;
        if(strcmp(conf->port, device->name) == 0)
        {
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
                ellAdd(&samplers, &s->ListNode<Sampler>::node);
                ellAdd(&pdo_delegates, &s->ListNode<ProcessDataObserver>::node);
            }
        }
    }
    
    for(NODE * node = listFirst(&device->pdo_entry_mappings); node; node = node->next)
    {
        EC_PDO_ENTRY_MAPPING * mapping = (EC_PDO_ENTRY_MAPPING *)node;
        char * name = makeParamName(mapping);
        printf("createParam %s\n", name);
        createParam(name, asynParamInt32, PdoParam + n);
        mapping->pdo_entry->parameter = PdoParam[n];
        write_delegates[PdoParam[n]] = new ProcessDataWriteObserver(writeq, mapping);
        n++;
    }

    createParam("AL_STATE", asynParamInt32, &P_AL_STATE);
    createParam("ERROR_FLAG", asynParamInt32, &P_ERROR_FLAG);
    createParam("DISABLE", asynParamInt32, &P_DISABLE);
    setIntegerParam(P_DISABLE, 1);

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

    for(ELLNODE * node = ellFirst(&pdo_delegates); node; node = ellNext(node))
    {
        CastNode<ProcessDataObserver>(node)->on_pdo_message(pdo, size);
    }

    for(NODE * node = listFirst(&device->pdo_entry_mappings); node; node = node->next)
    {
        EC_PDO_ENTRY_MAPPING * mapping = (EC_PDO_ENTRY_MAPPING *)node;
        int32_t val = cast_int32(mapping, pdo->buffer, 0);
#if 0
        if(disable != lastDisable)
        {
            // need this to trigger an I/O interrupt
            // yes this is nasty, this is so that SDIS is activated
            // until ASYN can return alarms through I/O interrupt
            // see tech-talk 11 May 2011
            setIntegerParam(mapping->pdo_entry->parameter, 0);
            setIntegerParam(mapping->pdo_entry->parameter, 1);
        }
        else
        {
            setIntegerParam(mapping->pdo_entry->parameter, val);
        }
#endif
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
    if(write_delegates[pasynUser->reason])
    {
        return write_delegates[pasynUser->reason]->writeInt32(pasynUser, value);
    }
    return status;
}

int init_unpack(ENGINE_USER * usr, char * buffer, int size)
{
    EC_CONFIG * cfg = usr->config;
    int ofs = 0;
    int tag = unpack_int(buffer, &ofs);
    assert(tag == MSG_CONFIG);
    int scanner_config_size = unpack_int(buffer, &ofs);
    read_config2(buffer + ofs, scanner_config_size, cfg);
    ofs += scanner_config_size;
    int mapping_config_size = unpack_int(buffer, &ofs);
    parseEntriesFromBuffer(buffer + ofs, mapping_config_size, cfg);

    NODE * node;
    for(node = listFirst(&cfg->devices); node; node = node->next)
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        printf("%s\n", device->name);
        NODE * node1;
        for(node1 = listFirst(&device->pdo_entry_mappings); node1; node1 = node1->next)
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
    NODE * node;
    int ndev = 0;
    for(node = listFirst(&cfg->devices); node; node = node->next)
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        int pdos = 0;
        for(NODE * node1 = listFirst(&device->pdo_entry_mappings); node1; node1 = node1->next)
        {
            pdos++;
        }
        ecAsyn * port = new ecAsyn(device, pdos, usr->writeq, ndev);
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
    return size > 0;
}

int pdo_data(ENGINE_USER * usr, char * buffer, int size)
{
    EC_MESSAGE * msg = (EC_MESSAGE *)buffer;
    assert(msg->tag == MSG_PDO);
    for(ELLNODE * node = ellFirst(&usr->ports); node; node = ellNext(node))
    {
        CastNode<ProcessDataObserver>(node)->on_pdo_message(&msg->pdo, size);
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

void makePorts(char * path, int max_message)
{
    ENGINE_USER * usr = (ENGINE_USER *)callocMustSucceed
        (1, sizeof(ENGINE_USER), "can't allocate socket engine private data");
    ellInit(&usr->ports);
    usr->master = new ecMaster("MASTER0");
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

    void ecAsynRegistrar(void)
    {
        ellInit(&sampler_configs);
        iocshRegister(&InitFuncDef,InitCallFunc);
        iocshRegister(&SamplerFuncDef,SamplerCallFunc);
    }
    epicsExportRegistrar(ecAsynRegistrar);
}
