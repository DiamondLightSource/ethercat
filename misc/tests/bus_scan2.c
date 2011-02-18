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

enum { MAX_MONITORS = 1000 };
enum { MAX_WRITES = 1000 };
enum { PRIO_LOW = 0, PRIO_HIGH = 60 };
enum { MAX_CLIENTS = 10 };
enum { MAX_MESSAGE = 128 };
enum { CLIENT_Q_CAPACITY = 10 };
enum { WORK_Q_CAPACITY = 10 };

rtMessageQueueId clientq[MAX_CLIENTS] = { NULL };
rtMessageQueueId workq = NULL;
queue_t * monitorq = NULL;
queue_t * writeq = NULL;

field_t * create_default_monitors(ethercat_device_config * chain, int client)
{
    int MAX_ROUTE = 1000;
    field_t * lookup_route = calloc(MAX_ROUTE, sizeof(field_t));
    int route = 0;

    monitor_request req = {0};

    ethercat_device_config * c;
    for(c = chain; c != NULL; c = c->next)
    {
        ethercat_device * d = c->dev;
        req.tag = MSG_MONITOR;
        req.vaddr = c->vaddr;
        req.client = client;

        int j;
        for(j = 0; j < d->n_fields; j++)
        {
            req.route = route;
            lookup_route[route] = d->fields[j];
            strncpy(req.name, d->fields[j].name, sizeof(req.name)-1);
            rtMessageQueueSend(workq, &req, sizeof(req));
            route++;
        }
    }
    return lookup_route;
}

void reader_task2(void * usr)
{
    ethercat_device_config * chain = (ethercat_device_config *)usr;
    create_default_monitors(chain, 1);
    char msg[MAX_MESSAGE];
    int count = 0;
    while(1)
    {
        rtMessageQueueReceive(clientq[1], msg, sizeof(msg));
        if(count % 1000 == 0)
            printf("black hole client\n");
        count++;
    }
}

void reader_task(void * usr)
{

    ethercat_device_config * chain = (ethercat_device_config *)usr;
    field_t * fields = create_default_monitors(chain, 0);
    char msg[MAX_MESSAGE];
    monitor_response * r = (monitor_response *)msg;
    while(1)
    {
        rtMessageQueueReceive(clientq[0], msg, sizeof(msg));
        int s;
        for(s = 0; s < r->length; s++)
        {
            printf("(%d:%d:%d)%s[%d] %d\n", 
                   r->vaddr, r->client, r->route, fields[r->route].name, s, r->value[s]);
        }
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
    uint8_t * pd = task->pd;
    struct timespec wakeupTime;
    char msg[MAX_MESSAGE] = {0};
    int * tag = (int *)msg;
    int tick = 0;
    while(1)
    {
        rtMessageQueueReceive(workq, msg, sizeof(msg));
        if(tag[0] == MSG_MONITOR)
        {
            monitor_request * req = (monitor_request *)msg;
            ec_addr addr = {0};
            addr.route = req->route;
            addr.vaddr = req->vaddr;
            addr.period = 100;
            addr.client = req->client;
            strncpy(addr.name, req->name, sizeof(addr.name));
            printf("subscription %s\n", req->name);
            queue_put(monitorq, &addr);
        }
        else if(tag[0] == MSG_WRITE)
        {
            write_request * wr = (write_request *)msg;
            ec_addr e;
            strncpy(e.name, wr->name, sizeof(e.name));
            e.route = wr->route;
            e.vaddr = wr->vaddr;
            e.client = wr->client;
            e.value = wr->value;
            queue_put(writeq, &e);
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
                    monitor_response reply = { 0 };
                    reply.tag = MSG_DATA;
                    reply.client = 0;
                    reply.vaddr = e.vaddr;
                    reply.route = e.route;
                    reply.length = f->size / sizeof(int32_t);
                    ethercat_device_read_field(f, reply.value, 
                                               sizeof(reply.value));

                    if(e.tick == e.period)
                    {
                        int size = (char *)(&reply.value[reply.length]) 
                            - (char *)&reply;
                        rtMessageQueueSend(clientq[e.client], &reply, size);
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

    workq = rtMessageQueueCreate(WORK_Q_CAPACITY, MAX_MESSAGE);

    int c;
    for(c = 0; c < MAX_CLIENTS; c++)
    {
        clientq[c] = rtMessageQueueCreate(CLIENT_Q_CAPACITY, 
                                          MAX_MESSAGE);
    }

    task_config config = { master, domain, chain };
    config.pd = ecrt_domain_data(domain);
    assert(config.pd);

    timer_usr t = { PERIOD_NS, workq };

    rtThreadCreate("reader", PRIO_LOW,  0, reader_task, chain );
    rtThreadCreate("reader", PRIO_LOW,  0, reader_task2, chain );
    rtThreadCreate("cyclic", PRIO_HIGH, 0, cyclic_task, &config);
    rtThreadCreate("timer",  PRIO_HIGH, 0, timer_task, &t);

    int n = 0;
    write_request wr = { MSG_WRITE, 0, 200, 0, "output0" };
    while(1)
    {
        wr.value = n % 2000;
        usleep(1000);
        rtMessageQueueSend(workq, &wr, sizeof(wr));
        n++;
    }

    xmlCleanupParser();
    return 0;
}

