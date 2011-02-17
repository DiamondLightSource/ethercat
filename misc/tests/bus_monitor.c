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

/* globals */

queue_t * monitorq = NULL;
queue_t * writeq = NULL;

void create_default_monitors(ethercat_device_config * chain)
{
    ec_addr addr = {0};
    ethercat_device_config * c;
    for(c = chain; c != NULL; c = c->next)
    {
        ethercat_device * d = c->dev;
        addr.tag = 0;
        addr.period = 100;
        addr.vaddr = c->vaddr;
        int j;
        for(j = 0; j < d->n_fields; j++)
        {
            strncpy(addr.name, d->fields[j].name, sizeof(addr.name)-1);
            queue_put(monitorq, &addr);
        }
    }
}

void show_monitor(field_t * f, ec_addr * e, char * buf)
{
    if(strncmp(f->name, "value", 5) == 0 && e->vaddr == 300)
    {
        int s;
        int16_t * val = (int16_t *)buf;
        for(s = 0; s < 10; s++)
        {
            printf("(%d)%s[%d] %d\n", e->vaddr, f->name, s, val[s]);
        }
    }
    else
    {
        uint32_t * val = (uint32_t *)buf;
        printf("(%d)%s %u\n", e->vaddr, f->name, val[0]);
    }
}

void cyclic(ec_master_t * master, ec_domain_t * domain, 
            ethercat_device_config * chain)
{

    create_default_monitors(chain);
    
    ec_addr writes[] = 
    {
        { 0, 200, "output0", 0 },
    };

    printf("activating\n");

    ecrt_master_activate(master);

    uint8_t * pd = ecrt_domain_data(domain);
    assert(pd);

    int n = 0;
    
    struct timespec wakeupTime;
    struct timespec cycletime = {0, PERIOD_NS};
    
    clock_gettime(CLOCK_MONOTONIC, &wakeupTime);

    while(1)
    {
        
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeupTime, NULL);
        wakeupTime = timespec_add(wakeupTime, cycletime);
        
        ecrt_master_application_time(master, TIMESPEC2NS(wakeupTime));
        ecrt_master_sync_reference_clock(master);
        ecrt_master_sync_slave_clocks(master);

        ecrt_master_receive(master);
        ecrt_domain_process(domain);

        // write test
        writes[0].value = n % 2000;
        queue_put(writeq, &writes[0]);
        

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

        ethercat_device_config * c;
        for(c = chain; c != NULL; c = c->next)
        {
            ethercat_device * d = c->dev;
            // custom processing 
            // (default is metadata update)
            if(d->process != NULL)
            {
                d->process(d, pd);
            }
            // process stage, copy into buffer
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
        int fired = 0;
        while(size > 0)
        {
            ec_addr e;
            queue_get(monitorq, &e);
            field_t * f = find_field(chain, &e);
            if(f != NULL)
            {
                char buf[1024] = {0};
                ethercat_device_read_field(f, buf, sizeof(buf));
                if(e.tick == e.period)
                {
                    show_monitor(f, &e, buf);
                    e.tick = 0;
                    fired = 1;
                }
                else
                {
                    e.tick++;
                }
            }
            queue_put(monitorq, &e);
            size--;
        }
        if(fired)
        {
            printf("\n");
        }

        ecrt_domain_queue(domain);
        ecrt_master_send(master);

        n++;
        
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
    cyclic(master, domain, chain);
    
    xmlCleanupParser();
    return 0;
}
