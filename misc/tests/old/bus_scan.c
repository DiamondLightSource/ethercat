#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include "ecrt.h"
#include "rtutils.h"

#include "ethercat_device.h"
#include "EL3162.h"
#include "EL3702.h"
#include "EL4102.h"
#include "EK1101.h"

#include "master_device.h"

#include "queue.h"

enum { MASTER_VADDR = 32768 };

int ethercat_device_read_field(ethercat_device * self, field_t * f, void * buf, int max_buf)
{
    if(max_buf >= f->size)
    {
        memcpy(buf, (char *)self + f->offset, f->size);
        return f->size;
    }
    return 0;
}

field_t * ethercat_device_get_field(ethercat_device * self, char * name)
{
    field_t * f;
    if(name == NULL)
    {
        return NULL;
    }
    for(f = self->fields; f->name != NULL; f++)
    {
        if(strcmp(f->name, name) == 0)
        {
            return f;
        }
    }
    return NULL;
}

typedef struct action action;

struct action
{
    int tag;
    int device;
    int channel;
    uint64_t value;
};

enum { MAX_CLIENTS = 128 };
enum { MAX_MONITORS = 1024 };

rtMessageQueueId clients[MAX_CLIENTS] = { NULL };

queue_t * monitorq = NULL;

ethercat_device * master_dev = NULL;

ethercat_constructor device_factory[] = 
{
    {"EL3162", EL3162_init},
    {"EK1101", EK1101_init},
    {"EL4102", EL4102_init},
    {"EL3702", EL3702_init},
    { NULL }
};

// chain configuration file

ethercat_device_config chain[] = 
{
    {"EK1101", 100, 0, 0},
    {"EL4102", 200, 0, 1},
    {"EL3702", 300, 0, 2, "10"},
    {"EL3162", 400, 0, 3},
    { NULL }
};

// lookup by virtual address
ethercat_device * get_device(int vaddr)
{
    ethercat_device_config * c;
    if(vaddr == MASTER_VADDR)
    {
        return master_dev;
    }
    for(c = chain; c->name != NULL; c++)
    {
        if(c->vaddr == vaddr)
        {
            return c->dev;
        }
    }
    return NULL;
}

ec_master_t * master = NULL;
ec_domain_t * domain = NULL;
uint8_t * pd = NULL;

rtMessageQueueId workq = NULL;
rtMessageQueueId dataq = NULL;
rtMessageQueueId replyq = NULL;

#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

enum { MSG_TICK = 0, MSG_WRITE, MSG_CYCLE, MSG_MONITOR, MSG_ASYN };

void timer_task(void * usr)
{
    struct timespec wakeupTime;
    struct timespec cycletime = {0, PERIOD_NS};
    int msg[4] = { MSG_TICK };
    clock_gettime(CLOCK_MONOTONIC, &wakeupTime);
    while(1)
    {
        wakeupTime = timespec_add(wakeupTime, cycletime);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeupTime, NULL);
        msg[1] = wakeupTime.tv_sec;
        msg[2] = wakeupTime.tv_nsec;
        rtMessageQueueSendPriority(workq, msg, sizeof(msg));
        rtMessageQueueSendPriority(dataq, msg, sizeof(msg));
    }
}

typedef struct dispatch_entry dispatch_entry;
struct dispatch_entry
{
    int tag;
    int client;
    int vaddr;
    int reason;
    char * name;
    field_t * field;
    ethercat_device * dev;
};

// broken because it can't search by alias
int get_slave_info(int pos)
{
        ec_slave_info_t info;
        ecrt_master_get_slave(master, pos, &info);
        return info.al_state;
}

// dispatch ASYN style packets with reason
void dispatch(dispatch_entry * e)
{
    int header_size = 4 * sizeof(int);
    char buffer[2048];
    int * tag = (int *)buffer;
    tag[0] = MSG_ASYN;
    tag[1] = e->reason;

    if(e->field != NULL)
    {
        if(e->dev != NULL && e->dev->read_field != NULL)
        {
            int data_size = e->dev->read_field(
                e->dev, e->field, &buffer[header_size], 
                sizeof(buffer) - header_size);
            rtMessageQueueTrySend(clients[e->client], buffer, 
                                  header_size + data_size);
        }

    }
    
}

void cyclic_task(void * usr)
{
    int n;
    char msg[2048];
    int * tag = (int *)msg;
    int count = 0;
    struct timespec now;

    while(1) 
    {
        rtMessageQueueReceive(workq, msg, sizeof(msg));
        
        if(tag[0] == MSG_WRITE)
        {
            action * act = (action *)msg;
            ethercat_device * dev = chain[act->device].dev;
            dev->write(dev, act->channel, act->value);
            continue;
        }
        
        if(tag[0] == MSG_MONITOR)
        {
            dispatch_entry * e = (dispatch_entry *)msg;
            e->dev = get_device(e->vaddr);
            if(e->dev != NULL && e->dev->get_field != NULL)
            {
                e->field = e->dev->get_field(e->dev, e->name);
                if(e->field != NULL)
                {
                    printf("vaddr(%d) found field %s\n", e->vaddr, e->field->name);
                }
            }
            queue_put(monitorq, e);
            continue;
        }
        
        clock_gettime(CLOCK_MONOTONIC, &now);
        ecrt_master_application_time(master, TIMESPEC2NS(now));
        ecrt_master_sync_reference_clock(master);
        ecrt_master_sync_slave_clocks(master);
        
        ecrt_master_receive(master);
        ecrt_domain_process(domain);
        
        master_dev->process(master_dev, NULL);

        for(n = 0; chain[n].name != NULL; n++)
        {
            if(chain[n].dev)
            {
                // problem here with aliases
                chain[n].dev->alarm = get_slave_info(chain[n].pos);
                chain[n].dev->process(chain[n].dev, pd);
            }
        }
        
        int size = queue_size(monitorq);
        while(size > 0)
        {
            dispatch_entry e;
            queue_get(monitorq, &e);

            dispatch(&e);

            queue_put(monitorq, &e);
            size--;
        }
        
        ecrt_domain_queue(domain);
        ecrt_master_send(master);
        
        tag[0] = MSG_CYCLE;
        tag[1] = count++;
        rtMessageQueueTrySend(clients[1], msg, 2 * sizeof(int));
        
    }
}

void output_task(void * usr)
{
    action act;

    act.tag = MSG_WRITE;
    act.device = 1;
    act.channel = 0;

    int msg[4];
    int n = 0;
    double freq = 20;
    while(1)
    {
        double value = (sin(2.0*M_PI*freq*n++/1000) + 1)/2;
        rtMessageQueueReceive(dataq, msg, sizeof(msg));
        act.value = (uint64_t)(value * 0x2000);
        rtMessageQueueSend(workq, &act, sizeof(act));
    }
}

void display_task(void * usr)
{

    // display with user data pointer

    char msg[2048];
    int * tag = (int *)msg;
    int client = 1;
    
    enum { REASON_ID, REASON_CYCLE, REASON_VALUE, REASON_VALUE_OVR, REASON_ALARM, 
           REASON_MASTER1, REASON_MASTER2, REASON_MASTER3 };

    dispatch_entry entries[] = 
    {
        { MSG_MONITOR, client, 100,   REASON_ID, "ID", NULL },
        { MSG_MONITOR, client, 300,   REASON_CYCLE, "cycle0", NULL },
        { MSG_MONITOR, client, 300,   REASON_VALUE_OVR, "value0", NULL },
        { MSG_MONITOR, client, 400,   REASON_VALUE, "value0", NULL },
        { MSG_MONITOR, client, 300,   REASON_ALARM, "alarm", NULL },
        { MSG_MONITOR, client, MASTER_VADDR, REASON_MASTER1, "slaves", NULL },
        { MSG_MONITOR, client, MASTER_VADDR, REASON_MASTER2, "alarm", NULL },
        { MSG_MONITOR, client, MASTER_VADDR, REASON_MASTER3, "link", NULL },
        { -1 }
    };

    dispatch_entry * d;
    for(d = entries; d->tag != -1; d++)
    {
        rtMessageQueueSend(workq, d, sizeof(dispatch_entry));
    }

    while(1)
    {
        rtMessageQueueReceive(clients[client], msg, sizeof(msg));

        if(tag[0] == MSG_CYCLE)
        {
            printf("bus cycle %d\n", tag[1]);
        }
        else if(tag[0] == MSG_ASYN)
        {
            int reason = tag[1];
            dispatch_entry * d;
            for(d = entries; d->tag != -1; d++)
            {
                int16_t * value = (int16_t *)&tag[4];
                if(d->reason == reason)
                {
                    if(reason == REASON_VALUE_OVR)
                    {
                        int s;
                        for(s = 0; s < 10; s++)
                        {
                            printf("device(%d) %s[%d] %d\n", d->vaddr, d->name, s, value[s]);
                        }
                    }
                    // maybe switch on data size?
                    else if(reason == REASON_ALARM)
                    {
                        uint8_t * value = (uint8_t *)&tag[4];
                        printf("device(%d) %s %d\n", d->vaddr, d->name, *value);
                    }
                    else
                    {
                        printf("device(%d) %s %d\n", d->vaddr, d->name, *value);
                    }
                    break;
                }
            }
        }
    }
}

void * device_init(ec_master_t * master, ec_domain_t * domain, ethercat_device_config * d)
{
    int n;
    for(n = 0; device_factory[n].name != NULL; n++)
    {
        if(strcmp(device_factory[n].name, d->name) == 0)
        {
            d->dev = device_factory[n].init(master, domain, d->alias, d->pos, d->usr);
            d->dev->show(d->dev);
            break;
        }
    }
    if(d->dev == NULL)
    {
        printf("not found: device %s\n", d->name);
    }
    return d->dev;
}

int main(int argc, char **argv)
{

    master = ecrt_request_master(0);
    assert(master);

    domain = ecrt_master_create_domain(master);
    assert(domain);

    int n;
    for(n = 0; chain[n].name != NULL; n++)
    {
        device_init(master, domain, &chain[n]);
    }

    master_dev = master_device_init(master, domain);

    printf("Activating master...\n");
    assert(ecrt_master_activate(master) == 0);
    
    pd = ecrt_domain_data(domain);
    assert(pd);
    
    dataq = rtMessageQueueCreate(10, 128);
    workq = rtMessageQueueCreate(10, 128);
    replyq = rtMessageQueueCreate(1, 128);

    monitorq = queue_init(MAX_MONITORS, sizeof(dispatch_entry));

    clients[1] = rtMessageQueueCreate(10, 2048);

    enum { HIGH = 60, MED = 30 };

    rtThreadCreate("timer",  HIGH, 0, timer_task,  NULL);
    rtThreadCreate("cyclic", HIGH, 0, cyclic_task, NULL);
    rtThreadCreate("output", MED,  0, output_task, NULL);
    rtThreadCreate("display", MED,  0, display_task, NULL);
    
    int msg[16];
    while(1)
    {
        rtMessageQueueReceive(replyq, msg, sizeof(msg));
        int n;
        for(n = 0; n < 16; n++)
        {
            printf("%d ", msg[n]);
        }
        printf("\n");
        //usleep(10000);
    }
    
    return 0;
}

/* 
   build full test system with standalone client
   using socket in other thread
   put timer task in library
*/
