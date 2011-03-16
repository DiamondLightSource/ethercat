#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <time.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <ecrt.h>
#include <unistd.h>

#include "rtutils.h"

#include "ethercat_device2.h"
#include "configparser.h"

#include "messages.h"
#include "timer.h"

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
enum { MAX_CLIENTS = 100 };
enum { MAX_MESSAGE = 128 };
enum { CLIENT_Q_CAPACITY = 10 };
enum { WORK_Q_CAPACITY = 10 };

rtMessageQueueId clientq[MAX_CLIENTS] = { NULL };
rtMessageQueueId workq = NULL;

void reader_task(void * usr)
{
    int id = (int)usr;
    char msg[MAX_MESSAGE];
    int count = 0;
    while(1)
    {
        rtMessageQueueReceive(clientq[id], msg, sizeof(msg));
        if(count % 500 == 0)
        {
            short * val = (short *)(msg + 13);
            // unpack switch
            printf("%d: %d\n", id, *val);
        }
        count++;
    }
}

typedef struct
{
    ec_master_t * master;
    ec_domain_t * domain;
    ethercat_device_config * chain;
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
            int client;
            for(client = 0; client < MAX_CLIENTS; client++)
            {
                rtMessageQueueSendNoWait(clientq[client], pd, pdo_size);
            }
            
            ecrt_domain_queue(domain);
            ecrt_master_send(master);
            
            tick++;
        }

    }

}

int main(int argc, char **argv)
{
    LIBXML_TEST_VERSION; 

    ethercat_device * devs = parse_device_file("config.xml");
    ethercat_device_config * chain = parse_chain_file("chain.xml");
    
    ec_master_t * master = NULL;
    ec_domain_t * domain = NULL;

    master = ecrt_request_master(0);
    assert(master);
    domain = ecrt_master_create_domain(master);
    assert(domain);

    initialize_chain(chain, devs, master, domain);

    ethercat_device_config * c;
    for(c = chain; c != NULL; c = c->next)
    {
        ethercat_device * d = c->dev;
        printf("%s\n", c->name);
        pdo_entry_type * p;
        for(p = d->regs; p != NULL; p = p->next)
        {
            int top = p->pdo_offset[p->length-1] + (p->entry->bit_length-1) / 8 + 1;
            if(top > pdo_size)
            {
                pdo_size = top;
            }
            printf("%s %d %d %d %d\n", 
                   p->name, p->pdo_offset[0], p->pdo_bit[0], p->entry->bit_length, p->length);
        }
    }
    printf("pdo memory size %d\n", pdo_size);

    ecrt_master_activate(master);

    workq = rtMessageQueueCreate(WORK_Q_CAPACITY, MAX_MESSAGE);

    int client;
    for(client = 0; client < MAX_CLIENTS; client++)
    {
        clientq[client] = rtMessageQueueCreate(CLIENT_Q_CAPACITY, 
                                               MAX_MESSAGE);
        rtThreadCreate("reader", PRIO_LOW,  0, reader_task, (void *)client);
    }

    task_config config = { master, domain, chain };
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
        //rtMessageQueueSend(workq, &wr, sizeof(wr));
        n++;
    }

    xmlCleanupParser();
    return 0;
}
