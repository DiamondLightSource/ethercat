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

// packing stuff
int pdo_data(char * buffer, int size);



enum { PERIOD_NS = 1000000 };
#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

typedef struct
{
    EC_CONFIG * config;
    char * config_buffer;
    ec_master_t * master;
    ec_domain_t * domain;
    int pdo_size;
    uint8_t * pd;
} SCANNER;

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

enum { PRIO_LOW = 0, PRIO_HIGH = 60 };
enum { MAX_CLIENTS = 2 };
enum { MAX_MESSAGE = 16384 };
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
            //short * val = (short *)(msg);
            // unpack switch
            //printf("read check thread %d: value %d size %d\n", args->id, *val, sz);
            pdo_data(msg, sz);
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

void cyclic_task(void * usr)
{
    SCANNER * scanner = (SCANNER *)usr;
    ec_master_t * master = scanner->master;
    ec_domain_t * domain = scanner->domain;
    uint8_t * pd = scanner->pd;
    struct timespec wakeupTime;
    char msg[MAX_MESSAGE] = {0};
    int * tag = (int *)msg;
    int tick = 0;

    uint8_t * write_cache = calloc(scanner->pdo_size, sizeof(char));
    uint8_t * write_mask = calloc(scanner->pdo_size, sizeof(char));

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
                if(ofs + n >= scanner->pdo_size)
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
            for(n = 0; n < scanner->pdo_size; n++)
            {
                pd[n] &= ~write_mask[n];
                pd[n] |= write_cache[n];
            }
            memset(write_mask, 0, scanner->pdo_size);
            
            // distribute PDO
            int client;
            for(client = 0; client < MAX_CLIENTS; client++)
            {
                rtMessageQueueSendNoWait(clientq[client], pd, scanner->pdo_size);
            }
            
            ecrt_domain_queue(domain);
            ecrt_master_send(master);
            
            tick++;
        }

    }

}

// why is ecrt_domain_size not defined for userspace?
void adjust_pdo_size(SCANNER * scanner, int offset, int bits)
{
    int bytes = (bits-1) / 8 + 1;
    int top = offset + bytes;
    if(top > scanner->pdo_size)
    {
        scanner->pdo_size = top;
    }
}

int debug = 1;

int device_initialize(SCANNER * scanner, EC_DEVICE * device)
{
    ec_slave_config_t * sc = ecrt_master_slave_config(
        scanner->master, 0, device->position, device->device_type->vendor_id, 
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
                int n;
                EC_PDO_ENTRY * pdo_entry = (EC_PDO_ENTRY *)node2;
                if(debug)
                {
                    printf("PDO ENTRY:    name \"%s\" index %x subindex %x bits %d\n", 
                           pdo_entry->name, pdo_entry->index, pdo_entry->sub_index, pdo_entry->bits);
                }
                /* 
                   scalar entries are added automatically, just add oversampling extensions
                */
                if(pdo_entry->oversampling)
                {
                    for(n = 1; n < device->oversampling_rate; n++)
                    {
                        printf("mapping %x\n", pdo_entry->index + n * pdo_entry->bits);
                        assert(ecrt_slave_config_pdo_mapping_add(
                                   sc, pdo->index, 
                                   pdo_entry->index + n * pdo_entry->bits,
                                   pdo_entry->sub_index, pdo_entry->bits) == 0);
                    }
                }
            }
        }
    }
    
    /* second pass, assign mappings */
    for(node0 = listFirst(&device->device_type->sync_managers); node0; node0 = node0->next)
    {
        EC_SYNC_MANAGER * sync_manager = (EC_SYNC_MANAGER *)node0;
        NODE * node1;
        for(node1 = listFirst(&sync_manager->pdos); node1; node1 = node1->next)
        {
            EC_PDO * pdo = (EC_PDO *)node1;
            NODE * node2;
            for(node2 = listFirst(&pdo->pdo_entries); node2; node2 = node2->next)
            {
                int n;
                EC_PDO_ENTRY * pdo_entry = (EC_PDO_ENTRY *)node2;
                EC_PDO_ENTRY_MAPPING * pdo_entry_mapping = calloc(1, sizeof(EC_PDO_ENTRY_MAPPING));
                pdo_entry_mapping->offset = 
                    ecrt_slave_config_reg_pdo_entry(
                        sc, pdo_entry->index, pdo_entry->sub_index,
                        scanner->domain, (unsigned int *)&pdo_entry_mapping->bit_position);
                adjust_pdo_size(scanner, pdo_entry_mapping->offset, pdo_entry->bits);
                if(pdo_entry->oversampling)
                {
                    for(n = 1; n < device->oversampling_rate; n++)
                    {
                        int bit_position;
                        int ofs = ecrt_slave_config_reg_pdo_entry(
                            sc, pdo_entry->index + n * pdo_entry->bits, pdo_entry->sub_index,
                            scanner->domain, (unsigned int *)&bit_position);
                        printf("pdo entry reg %08x %04x %d\n", 
                               pdo_entry->index + n * pdo_entry->bits, 
                               pdo_entry->sub_index, ofs);
                        assert(ofs == pdo_entry_mapping->offset + n * pdo_entry->bits / 8);
                        assert(bit_position == pdo_entry_mapping->bit_position);
                        adjust_pdo_size(scanner, ofs, pdo_entry->bits);
                    }
                }
                pdo_entry_mapping->index = pdo_entry->index;
                pdo_entry_mapping->sub_index = pdo_entry->sub_index;
                listAdd(&device->pdo_entry_mappings, &pdo_entry_mapping->node);
            }
        }
    }

    if(device->oversampling_rate != 0)
    {
        printf("oversamping rate %d\n", device->oversampling_rate);
        ecrt_slave_config_dc(sc, device->device_type->oversampling_activate, PERIOD_NS / device->oversampling_rate,
            0, PERIOD_NS, 0);
    }

    return 0;
}

int init_unpack(char * data, int sz);

void pack_int(char * buffer, int * ofs, int value)
{
    printf("packing at %d\n", *ofs);
    *((int *)(buffer + *ofs)) = value;
    (*ofs)+=sizeof(int);
}

void pack_string(char * buffer, int * ofs, char * str)
{
    int sl = strlen(str) + 1;
    printf("string length %d\n", sl);
    pack_int(buffer, ofs, sl);
    strcpy(buffer + *ofs, str);
    (*ofs) += sl;
}

int ethercat_init(SCANNER * scanner)
{
    NODE * node;
    for(node = listFirst(&scanner->config->devices); node; node = node->next)
    {
        EC_DEVICE * device = (EC_DEVICE *)node;
        assert(device->device_type);
        printf("DEVICE:       name \"%s\" type \"%s\" position %d\n", \
               device->name, device->type_name, device->position);
        device_initialize(scanner, device);
    }
    printf("PDO SIZE:     %d\n", scanner->pdo_size);
    return 0;
}

SCANNER * start_scanner(char * filename, char * socketname)
{
    SCANNER * scanner = calloc(1, sizeof(SCANNER));
    scanner->config = calloc(1, sizeof(EC_CONFIG));
    scanner->config_buffer = load_config(filename);
    assert(scanner->config_buffer);
    read_config2(scanner->config_buffer, strlen(scanner->config_buffer), scanner->config);
    scanner->master = ecrt_request_master(0);
    scanner->domain = ecrt_master_create_domain(scanner->master);
    ethercat_init(scanner);
    return scanner;
}

void client_send_config(SCANNER * scanner)
{
    // send config to clients
    EC_CONFIG * cfg = scanner->config;
    char * sbuf = serialize_config(cfg);
    int msg_size = strlen(sbuf) + 1 + strlen(scanner->config_buffer) + 1 + 3 * sizeof(int);
    char * config_msg = calloc(msg_size, sizeof(char));
    int msg_ofs = 0;
    pack_int(config_msg, &msg_ofs, MSG_CONFIG);
    pack_string(config_msg, &msg_ofs, scanner->config_buffer);
    pack_string(config_msg, &msg_ofs, sbuf);
    init_unpack(config_msg, msg_size);
}

int main(int argc, char **argv)
{

    // start scanner
    SCANNER * scanner = start_scanner("scanner.xml", NULL);

    client_send_config(scanner);

    // activate master
    ecrt_master_activate(scanner->master);
    scanner->pd = ecrt_domain_data(scanner->domain);
    assert(scanner->pd);

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

    timer_usr t = { PERIOD_NS, workq };

    rtThreadCreate("cyclic", PRIO_HIGH, 0, cyclic_task, scanner);
    rtThreadCreate("timer",  PRIO_HIGH, 0, timer_task, &t);

    // write test data
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
        //rtMessageQueueSend(workq, &wr, sizeof(wr));
        n++;
    }

    return 0;
}

/*
TODO
1) unpack PDO into ASYN parameters -> need to read the device types and create the port parameters
2) proper logging macros

Want single process for testing, make asyn function that starts the scanner in a thread
connection handling state machine
create N clients (threads, message queues, etc...)
2 threads per client?

*/
