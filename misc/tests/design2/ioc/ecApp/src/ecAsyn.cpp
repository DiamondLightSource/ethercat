#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <epicsThread.h>
#include <epicsExport.h>
#include <asynPortDriver.h>
#include <iocsh.h>

#include "classes.h"
#include "parser.h"
#include "unpack.h"
extern "C"
{
#include "rtutils.h"
}
#include "msgsock.h"
#include "messages.h"

#include "ecAsyn.h"

struct PORT_NODE
{
    NODE node;
    ecAsyn * port;
};

struct ENGINE_USER
{
    ecMaster * master;
    LIST ports;
    EC_CONFIG * config;
    rtMessageQueueId config_ready;
    int count;
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
                   0) /* default stack size */
{
    printf("create master %s\n", name);
    createParam("Cycle", asynParamInt32, &P_Cycle);
    createParam("WorkingCounter", asynParamInt32, &P_WorkingCounter);
    createParam("WcState", asynParamInt32, &P_WcState);
}

ecAsyn::ecAsyn(EC_DEVICE * device, int pdos) :
    asynPortDriver(device->name,
                   1, /* maxAddr */
                   pdos + N_RESERVED_PARAMS, /* max parameters */
                   asynInt32Mask | asynDrvUserMask, /* interface mask*/
                   asynInt32Mask, /* interrupt mask */
                   0, /* non-blocking, no addresses */
                   1, /* autoconnect */
                   0, /* default priority */
                   0) /* default stack size */,
    device(device)
{
    printf("ecAsyn INIT %s PDOS %d\n", device->name, pdos);
    int * PdoParam = new int[pdos]; /* leak */
    int n = 0;
    NODE * node1;
    for(node1 = listFirst(&device->device_type->sync_managers); node1; node1 = node1->next)
    {
        EC_SYNC_MANAGER * sync = (EC_SYNC_MANAGER *)node1;
        NODE * node2;
        for(node2 = listFirst(&sync->pdos); node2; node2 = node2->next)
        {
            EC_PDO * pdo = (EC_PDO *)node2;
            NODE * node3;
            for(node3 = listFirst(&pdo->pdo_entries); node3; node3 = node3->next)
            {
                EC_PDO_ENTRY * entry = (EC_PDO_ENTRY *)node3;
                printf("createParam %s\n", pdo->name);
                createParam(pdo->name, asynParamInt32, PdoParam + n);
                entry->parameter = PdoParam[n];
                n++;
            }
        }
    }
}

void ecMaster::on_pdo_message(PDO_MESSAGE * pdo, int size)
{
    setIntegerParam(P_Cycle, pdo->cycle);
    setIntegerParam(P_WorkingCounter, pdo->working_counter);
    setIntegerParam(P_WcState, pdo->wc_state);
    callParamCallbacks();
}

void ecAsyn::on_pdo_message(PDO_MESSAGE * pdo, int size)
{
    for(NODE * node = listFirst(&device->pdo_entry_mappings); node; node = node->next)
    {
        EC_PDO_ENTRY_MAPPING * mapping = (EC_PDO_ENTRY_MAPPING *)node;
        int32_t val = cast_int32(mapping, pdo->buffer, 0);
        setIntegerParam(mapping->pdo_entry->parameter, val);
    }
    callParamCallbacks();
}

asynStatus ecAsyn::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    return asynPortDriver::writeInt32(pasynUser, value);
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
    for(node = listFirst(&cfg->devices); node; node = node->next)
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        int pdos = 0;
        NODE * node1;
        for(node1 = listFirst(&device->device_type->sync_managers); node1; node1 = node1->next)
        {
            EC_SYNC_MANAGER * sync = (EC_SYNC_MANAGER *)node1;
            NODE * node2;
            for(node2 = listFirst(&sync->pdos); node2; node2 = node2->next)
            {
                pdos++;
            }
        }
        PORT_NODE * pn = (PORT_NODE *)calloc(1, sizeof(PORT_NODE));
        pn->port = new ecAsyn(device, pdos);
        listAdd(&usr->ports, &pn->node);
    }
}

static int receive_config_on_connect(ENGINE * engine, int sock)
{
    ENGINE_USER * usr = (ENGINE_USER *)engine->usr;
    int ack = 0;
    int size = rtSockReceive(sock, engine->receive_buffer, engine->max_message);
    if(size > 0)
    {
        init_unpack(usr, engine->receive_buffer, size);
        readConfig(usr);
        rtMessageQueueSend(usr->config_ready, &ack, sizeof(int));
    }
    return size > 0;
}

int pdo_data(ENGINE_USER * usr, char * buffer, int size)
{
    EC_MESSAGE * msg = (EC_MESSAGE *)buffer;
    assert(msg->tag == MSG_PDO);
    usr->master->on_pdo_message(&msg->pdo, size);
    for(NODE * node = listFirst(&usr->ports); node; node = node->next)
    {
        PORT_NODE * pn = (PORT_NODE *)node;
        pn->port->on_pdo_message(&msg->pdo, size);
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
    usleep(1000000);
    *(int *)server->send_buffer = MSG_HEARTBEAT;
    return sizeof(int);
}

void makePorts(char * path, int max_message)
{
    ENGINE_USER * usr = (ENGINE_USER *)calloc(1, sizeof(ENGINE_USER));
    usr->master = new ecMaster("MASTER0");
    usr->config_ready = rtMessageQueueCreate(1, sizeof(int));
    usr->config = (EC_CONFIG *)calloc(1, sizeof(EC_CONFIG));
    
    ENGINE * engine = new_engine(max_message);
    engine->path = path;
    engine->connect = client_connect;
    engine->on_connect = receive_config_on_connect;
    engine->send_message = ioc_send;
    engine->receive_message = ioc_receive;
    engine->usr = usr;
    engine_start(engine);

    int ack;
    rtMessageQueueReceive(usr->config_ready, &ack, sizeof(int));

}

extern "C"
{
    /* EPICS iocsh shell commands */
    static const iocshArg InitArg[] = { { "path",       iocshArgString },
                                        { "maxmessage", iocshArgInt    } };
    static const iocshArg * const InitArgs[] = { &InitArg[0], &InitArg[1] };
    static const iocshFuncDef initFuncDef = {"ecAsynInit", 2, InitArgs};
    static void initCallFunc(const iocshArgBuf * args)
    {
        makePorts(args[0].sval, args[1].ival);
    }
    void ecAsynRegistrar(void)
    {
        iocshRegister(&initFuncDef,initCallFunc);
    }
    epicsExportRegistrar(ecAsynRegistrar);
}
