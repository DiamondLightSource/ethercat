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
#include "queue.h"

#include "messages.h"
#include "timer.h"

/* globals */

enum { MAX_CLIENTS = 10 };

rtMessageQueueId clientq[MAX_CLIENTS] = { NULL };
rtMessageQueueId workq = NULL;
rtMessageQueueId dataq = NULL;
rtMessageQueueId replyq = NULL;
queue_t * monitorq = NULL;
queue_t * writeq = NULL;

void create_default_monitors(ethercat_device_config * chain)
{
    int reason = 0;
    ec_addr addr = {0};
    ethercat_device_config * c;
    for(c = chain; c != NULL; c = c->next)
    {
        ethercat_device * d = c->dev;
        addr.tag = MSG_MONITOR;
        addr.period = 100;
        addr.vaddr = c->vaddr;
        addr.client = 0;
        int j;
        for(j = 0; j < d->n_fields; j++)
        {
            addr.reason = reason++;
            strncpy(addr.name, d->fields[j].name, sizeof(addr.name)-1);
            rtMessageQueueSend(workq, &addr, sizeof(addr));
        }
    }
}

void reader_task(void * usr)
{
    char msg[1024];
    ec_addr_buf * reply = (ec_addr_buf *)msg;
    ec_addr * e = (ec_addr *)&reply->base;
    while(1)
    {
        rtMessageQueueReceive(clientq[0], msg, sizeof(msg));
        uint8_t  * value1 = (uint8_t  *)reply->buf;
        uint16_t * value2 = (uint16_t *)reply->buf;
        uint32_t * value4 = (uint32_t *)reply->buf;
        switch(e->datatype)
        {
        case 1:
            printf("(%d:%d:%d)%s %u\n", 
                   e->vaddr, e->client, e->reason, e->name, value1[0]);
            break;
        case 2:
            printf("(%d:%d:%d)%s %u\n", 
                   e->vaddr, e->client, e->reason, e->name, value2[0]);
            break;
        case 4:
            printf("(%d:%d:%d)%s %u\n", 
                   e->vaddr, e->client, e->reason, e->name, value4[0]);
            break;
        default:
            printf("(%d:%d:%d)%s %u\n", 
                   e->vaddr, e->client, e->reason, e->name, value2[0]);
            break;
        }
    }
}

void send_monitor(field_t * f, ec_addr * e, char * buf)
{
    rtMessageQueueSend(clientq[0], e, sizeof(ec_addr));
}

void show_monitor(field_t * f, ec_addr * e, char * buf)
{
    if(strncmp(e->name, "value", 5) == 0 && e->vaddr == 300)
    {
        int s;
        int16_t * val = (int16_t *)buf;
        for(s = 0; s < 10; s++)
        {
            printf("(%d:%d:%d)%s[%d] %d\n", 
                   e->vaddr, e->client, e->reason, e->name, s, val[s]);
            e->value = val[0];
        }
    }
    else
    {
        uint32_t * val = (uint32_t *)buf;
        printf("(%d:%d:%d)%s %u\n", 
               e->vaddr, e->client, e->reason, f->name, val[0]);
        e->value = val[0];
    }
}

typedef struct task_config task_config;
struct task_config
{
    ec_master_t * master;
    ec_domain_t * domain;
    ethercat_device_config * chain;
    uint8_t * pd;
};
    
void cyclic_task(void * usr)
{

    task_config * task = (task_config *)usr;

    ec_master_t * master = task->master;
    ec_domain_t * domain = task->domain;
    ethercat_device_config * chain = task->chain;
    
    printf("activating\n");

    uint8_t * pd = task->pd;
    
    struct timespec wakeupTime;
    
    char msg[1024] = {0};
    int * tag = (int *)msg;

    int tick = 0;

    while(1)
    {

        rtMessageQueueReceive(workq, msg, sizeof(msg));
        if(tag[0] == MSG_MONITOR)
        {
            ec_addr * e = (ec_addr *)msg;
            printf("subscription %s\n", e->name);
            queue_put(monitorq, e);
        }

        else if(tag[0] == MSG_WRITE)
        {
            ec_addr * e = (ec_addr *)msg;
            queue_put(writeq, e);
        }

        else if(tag[0] == MSG_TICK)
        {
        
            wakeupTime.tv_sec = tag[1];
            wakeupTime.tv_nsec = tag[2];

            ecrt_master_application_time(master, TIMESPEC2NS(wakeupTime));
            ecrt_master_sync_reference_clock(master);
            ecrt_master_sync_slave_clocks(master);
            
            ecrt_master_receive(master);
            ecrt_domain_process(domain);
            
            // writes
            while(queue_size(writeq))
            {
                ec_addr e;
                queue_get(writeq, &e);
                field_t * f = find_field(chain, &e);
                if(f != NULL)
                {
                    ethercat_device_write_field(
                        f, &e.value, sizeof(e.value));
                }
            }
            
            // processing
            ethercat_device_config * c;
            for(c = chain; c != NULL; c = c->next)
            {
                ethercat_device * d = c->dev;
                if(d->process != NULL)
                {
                    d->process(d, pd);
                }
                pdo_entry_type * r;
                for(r = d->regs; r != NULL; r = r->next)
                {
                    if(r->sync->dir == EC_DIR_INPUT)
                    {
                        read_pdo(r, pd, d->pdo_buffer);
                    }
                    else if(r->sync->dir == EC_DIR_OUTPUT)
                    {
                        write_pdo(r, pd, d->pdo_buffer);
                    }
                }
            }
            
            // monitors
            int size = queue_size(monitorq);
            while(size > 0)
            {
                ec_addr e;
                queue_get(monitorq, &e);
                field_t * f = find_field(chain, &e);
                if(f != NULL)
                {
                    ec_addr_buf reply = {0};
                    memcpy(&reply.base, &e, sizeof(e));
                    reply.base.tag = MSG_DATA;
                    reply.base.size = sizeof(ec_addr);
                    reply.base.size += ethercat_device_read_field(
                        f, reply.buf, sizeof(reply.buf));
                    reply.base.datatype = f->size;
                    if(e.tick == e.period)
                    {
                        rtMessageQueueSend(clientq[e.client], &reply, reply.base.size);
                        //show_monitor(f, &e, reply.buf);
                        e.tick = 0;
                    }
                    else
                    {
                        e.tick++;
                    }
                }
                queue_put(monitorq, &e);
                size--;
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

    enum { MAX_MONITORS = 1000 };
    enum { MAX_WRITES = 1000 };

    monitorq = queue_init(MAX_MONITORS, sizeof(ec_addr));
    writeq = queue_init(MAX_WRITES, sizeof(ec_addr));

    ethercat_device * devs = parse_device_file("config.xml");
    ethercat_device_config * chain = parse_chain_file("chain.xml");
    
    ec_master_t * master = NULL;
    ec_domain_t * domain = NULL;

    master = ecrt_request_master(0);
    assert(master);
    domain = ecrt_master_create_domain(master);
    assert(domain);

    initialize_chain(chain, devs, master, domain);

    ecrt_master_activate(master);

    dataq = rtMessageQueueCreate(10, 128);
    workq = rtMessageQueueCreate(10, 128);
    replyq = rtMessageQueueCreate(1, 128);

    int c;
    for(c = 0; c < MAX_CLIENTS; c++)
    {
        clientq[c] = rtMessageQueueCreate(10, 128);
    }

    enum { HIGH = 60, LOW = 0 };

    task_config config = { master, domain, chain };
    config.pd = ecrt_domain_data(domain);
    assert(config.pd);

    timer_usr t = { PERIOD_NS, workq };

    rtThreadCreate("reader", LOW,  0, reader_task, NULL );
    rtThreadCreate("cyclic", HIGH, 0, cyclic_task, &config);
    rtThreadCreate("timer",  HIGH, 0, timer_task, &t);

    create_default_monitors(chain);

    int n = 0;
    ec_addr e = { MSG_WRITE, 200, "output0" };
    while(1)
    {
        e.value = n % 2000;
        usleep(1000);
        rtMessageQueueSend(workq, &e, sizeof(e));
        n++;
    }

    char msg[1024];
    rtMessageQueueReceive(replyq, msg, sizeof(msg));

    xmlCleanupParser();
    return 0;
}

