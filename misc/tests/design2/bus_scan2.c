#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ecrt.h>

#include "rtutils.h"
#include "msgsock.h"
#include "messages.h"
#include "timer.h"
#include "classes.h"
#include "parser.h"

enum { PERIOD_NS = 1000000 };
#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

typedef struct
{
    int tag;
    int working_counter;
    int size;
    char buffer;
} message_pdo;

enum { WRITE_SIZE = 8 };

typedef struct
{
    int tag;
    int ofs;
    uint8_t data[WRITE_SIZE];
    uint8_t mask[WRITE_SIZE];
} message_write;

/* globals */

int pdo_size = 0;

enum { PRIO_LOW = 0, PRIO_HIGH = 60 };
enum { MAX_CLIENTS = 2 };
enum { MAX_MESSAGE = 13000 };
enum { CLIENT_Q_CAPACITY = 10 };
enum { WORK_Q_CAPACITY = 10 };

rtMessageQueueId clientq[MAX_CLIENTS] = { NULL };
rtMessageQueueId workq = NULL;

struct reader_args
{
    int id;
    int sock;
};

void socket_reader_task(void * usr)
{
    char msg[MAX_MESSAGE];
    struct reader_args * args = (struct reader_args *)usr;
    //printf("hello %d %d\n", args->id, args->sock);
    int count = 0;
    while(1)
    {
        int sz = rtSockReceive(args->sock, msg, sizeof(msg));
        if(count % 100 == 0 && args->id == 1)
        {
            short * val = (short *)(msg);
            // unpack switch
            printf("read check thread %d: value %d size %d\n", args->id, *val, sz);
        }
        count++;
    }
}

void reader_task(void * usr)
{
    struct reader_args * args = (struct reader_args *)usr;
    int id = args->id;
    char msg[MAX_MESSAGE];
    int count = 0;
    while(1)
    {
        int sz = rtMessageQueueReceive(clientq[id], msg, sizeof(msg));
        rtSockSend(args->sock, msg, sz);
        if(count % 500 == 0)
        {
            //short * val = (short *)(msg + 13);
            // unpack switch
            //printf("read check thread %d: value %d size %d\n", id, *val, sz);
        }
        count++;
    }
}

typedef struct
{
    ec_master_t * master;
    ec_domain_t * domain;
    uint8_t * pd;
} task_config;

void cyclic_task(void * usr)
{
    task_config * task = (task_config *)usr;
    ec_master_t * master = task->master;
    ec_domain_t * domain = task->domain;
    uint8_t * pd = task->pd;
    struct timespec wakeupTime;
    char msg[MAX_MESSAGE] = {0};
    int * tag = (int *)msg;
    int tick = 0;

    uint8_t * write_cache = calloc(pdo_size, sizeof(char));
    uint8_t * write_mask = calloc(pdo_size, sizeof(char));

    ec_domain_state_t domain_state;

    while(1)
    {
        rtMessageQueueReceive(workq, msg, sizeof(msg));

        if(tag[0] == MSG_WRITE)
        {
            message_write * wr = (message_write *)msg;
            // write with mask requires read-modify-write
            int n;
            int ofs = wr->ofs;
            for(n = 0; n < WRITE_SIZE; n++)
            {
                if(ofs + n >= pdo_size)
                {
                    break;
                }
                write_mask[ofs + n] |= wr->mask[n];
                write_cache[ofs + n] &= ~wr->mask[n];
                write_cache[ofs + n] |= wr->data[n];
            }
        }
        else if(tag[0] == MSG_TICK)
        {
            wakeupTime.tv_sec = tag[1];
            wakeupTime.tv_nsec = tag[2];
            
            ecrt_master_application_time(master, TIMESPEC2NS(wakeupTime));
            
            // do this how often?
            ecrt_master_sync_reference_clock(master);
            ecrt_master_sync_slave_clocks(master);
            
            ecrt_master_receive(master);
            ecrt_domain_process(domain);

            ecrt_domain_state(domain, &domain_state);
            
            // merge writes
            int n;
            for(n = 0; n < pdo_size; n++)
            {
                pd[n] &= ~write_mask[n];
                pd[n] |= write_cache[n];
            }
            memset(write_mask, 0, pdo_size);

            // client broadcast
            char buffer[MAX_MESSAGE];
            memcpy(buffer, pd, pdo_size);

            int client;
            for(client = 0; client < MAX_CLIENTS; client++)
            {
                //rtMessageQueueSendNoWait(clientq[client], pd, pdo_size);
                rtMessageQueueSendNoWait(clientq[client], buffer, MAX_MESSAGE);
            }
            
            ecrt_domain_queue(domain);
            ecrt_master_send(master);
            
            tick++;
        }

    }

}

int debug = 1;

int device_initialize(EC_DEVICE * device, ec_master_t * master, ec_domain_t * domain, int * pdo_size)
{
    ec_slave_config_t * sc = ecrt_master_slave_config(
        master, 0, device->position, device->device_type->vendor_id, 
        device->device_type->product_id);
    assert(sc);
    NODE * node0;
    for(node0 = listFirst(&device->device_type->sync_managers); node0; node0 = node0->next)
    {
        EC_SYNC_MANAGER * sync_manager = (EC_SYNC_MANAGER *)node0;
        if(debug)
        {
            printf("SYNC MANAGER: dir %d watchdog %d\n", sync_manager->direction, sync_manager->watchdog);
        }
        NODE * node1;
        for(node1 = listFirst(&sync_manager->pdos); node1; node1 = node1->next)
        {
            EC_PDO * pdo = (EC_PDO *)node1;
            if(debug)
            {
                printf("PDO:          index %x\n", pdo->index);
            }
            NODE * node2;
            for(node2 = listFirst(&pdo->pdo_entries); node2; node2 = node2->next)
            {
                EC_PDO_ENTRY * pdo_entry = (EC_PDO_ENTRY *)node2;
                if(debug)
                {
                    printf("PDO ENTRY:    name \"%s\" index %x subindex %x bits %d\n", 
                           pdo_entry->name, pdo_entry->index, pdo_entry->sub_index, pdo_entry->bits);
                }
                assert(ecrt_slave_config_pdo_mapping_add(
                           sc, pdo->index, 
                           pdo_entry->index,
                           pdo_entry->sub_index, pdo_entry->bits) == 0);
                EC_PDO_ENTRY_MAPPING * pdo_entry_mapping = calloc(1, sizeof(EC_PDO_ENTRY_MAPPING));
                pdo_entry_mapping->offset = 
                    ecrt_slave_config_reg_pdo_entry(
                        sc, pdo_entry->index, pdo_entry->sub_index,
                        domain, (unsigned int *)&pdo_entry_mapping->bit_position);
                pdo_entry_mapping->index = pdo_entry->index;
                pdo_entry_mapping->sub_index = pdo_entry->sub_index;
                int top = pdo_entry_mapping->offset + (pdo_entry->bits-1)/8 + 1;
                if(top > *pdo_size)
                {
                    *pdo_size = top;
                }
                listAdd(&device->pdo_entry_mappings, &pdo_entry_mapping->node);
            }
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    ec_master_t * master = ecrt_request_master(0);
    ec_domain_t * domain = ecrt_master_create_domain(master);

    // pass in the config structure...
    
    EC_CONFIG * cfg = calloc(1, sizeof(EC_CONFIG));
    
    read_config2("scanner.xml", cfg);

    /* initialize */
    NODE * node;
    for(node = listFirst(&cfg->devices); node; node = node->next)
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        assert(device->device_type);
        printf("DEVICE:       name \"%s\" type \"%s\" position %d\n", device->name, device->type_name, device->position);
        device_initialize(device, master, domain, &pdo_size);
    }
    printf("PDO SIZE:     %d\n", pdo_size);

    /* serialize PDO mapping */
    int scount = 1024*1024;
    char * sbuf = calloc(scount, sizeof(char));
    strncat(sbuf, "<entries>\n", scount-strlen(sbuf)-1);
    for(node = listFirst(&cfg->devices); node; node = node->next)
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        NODE * node1;
        for(node1 = listFirst(&device->pdo_entry_mappings); node1; node1 = node1->next)
        {
            EC_PDO_ENTRY_MAPPING * pdo_entry_mapping = (EC_PDO_ENTRY_MAPPING *)node1;
            char line[1024];
            snprintf(line, sizeof(line), "<entry device_position=\"%d\" index=\"0x%x\" sub_index=\"0x%x\" offset=\"%d\" bit=\"%d\" />\n", 
                     device->position, pdo_entry_mapping->index, pdo_entry_mapping->sub_index, pdo_entry_mapping->offset, 
                     pdo_entry_mapping->bit_position);
            strncat(sbuf, line, scount-strlen(sbuf)-1);
        }
    }
    strncat(sbuf, "</entries>\n", scount-strlen(sbuf)-1);

    /* deserialize PDO mapping */
    parseEntriesFromBuffer(sbuf, strlen(sbuf), cfg);

    /* check PDO mapping */
    for(node = listFirst(&cfg->devices); node; node = node->next)
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        printf("%s entries %d\n", device->name, device->pdo_entry_mappings.count);
    }
    
    ecrt_master_activate(master);

    workq = rtMessageQueueCreate(WORK_Q_CAPACITY, MAX_MESSAGE);

    int client;
    for(client = 0; client < MAX_CLIENTS; client++)
    {
        clientq[client] = rtMessageQueueCreate(CLIENT_Q_CAPACITY,
                                               MAX_MESSAGE);

        struct reader_args * args = (struct reader_args *)calloc(1, sizeof(struct reader_args));
        struct reader_args * args2 = (struct reader_args *)calloc(1, sizeof(struct reader_args));
        args->id = client;
        args2->id = client;

        int pair[2];
        assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
        args->sock = pair[0];
        args2->sock = pair[1];
        rtThreadCreate("reader", PRIO_LOW,  0, reader_task, args);
        rtThreadCreate("reader2", PRIO_LOW,  0, socket_reader_task, args2);
    }

    task_config config = { master, domain };
    config.pd = ecrt_domain_data(domain);
    assert(config.pd);

    timer_usr t = { PERIOD_NS, workq };

    rtThreadCreate("cyclic", PRIO_HIGH, 0, cyclic_task, &config);
    rtThreadCreate("timer",  PRIO_HIGH, 0, timer_task, &t);

    int n = 0;
    message_write wr;
    while(1)
    {
        wr.tag = MSG_WRITE;
        wr.ofs = 13;
        short * val = (short *)wr.data;
        *val = n;
        wr.mask[0] = 0xff;
        wr.mask[1] = 0xff;
        usleep(1000);
        rtMessageQueueSend(workq, &wr, sizeof(wr));
        n++;
    }

    return 0;
}

/*
TODO
1) load files into buffer, send to other task, unpack...
2) do oversampling modules
3) unpack PDO into ASYN parameters -> need to read the device types and create the port parameters
4) remove PDO and SYNC_MANAGER types (not needed for config or unpacking)
*/
