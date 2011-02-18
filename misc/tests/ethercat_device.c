#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <ecrt.h>
#include <string.h>

#include "ethercat_device2.h"

struct master_device
{
    ethercat_device base;
    int32_t slaves_responding;
    int32_t al_states;
    int32_t link_up;
    int32_t cycle;
};

int ethercat_device_read_field(field_t * f, void * buf, int max_buf)
{
    if(max_buf >= f->size)
    {
        memcpy(buf, f->buffer, f->size);
        return f->size;
    }
    return 0;
}

int ethercat_device_write_field(field_t * f, void * buf, int max_buf)
{
    if(max_buf >= f->size)
    {
        memcpy(f->buffer, buf, f->size);
        return f->size;
    }
    return 0;
}

void read_pdo(pdo_entry_type * r, uint8_t * pd, char * buffer)
{
    int s;
    int32_t * y = (int32_t *)(buffer + r->buffer_offset);
    switch(r->entry->bit_length)
    {
    case 1:
    {
        for(s = 0; s < r->length; s++)
        {
            *y = *(pd + r->pdo_offset[s]);
            *y &= 1U << r->pdo_bit[s];
            y++;
        }
    }
    break;
    case 8:
    {
        for(s = 0; s < r->length; s++)
        {
            *y++ = EC_READ_S8(pd + r->pdo_offset[s]);
        }
    }
    break;
    case 16:
    {
        for(s = 0; s < r->length; s++)
        {
            *y++ = EC_READ_S16(pd + r->pdo_offset[s]);
        }
    }
    break;
    case 32:
    {
        for(s = 0; s < r->length; s++)
        {
            *y++ = EC_READ_S32(pd + r->pdo_offset[s]);
        }
    }
    break;
    }
};

void write_pdo(pdo_entry_type * r, uint8_t * pd, char * buffer)
{
    // scalars only
    uint64_t x = 0;
    int s = 0;
    memcpy(&x, buffer + r->buffer_offset, r->buffer_size);
    switch(r->entry->bit_length)
    {
    case 8:
        EC_WRITE_U8(pd + r->pdo_offset[s], x);
        break;
    case 16:
        EC_WRITE_U16(pd + r->pdo_offset[s], x);
        break;
    case 32:
        EC_WRITE_U16(pd + r->pdo_offset[s], x);
        break;
    }
};

void process_master(ethercat_device * base, uint8_t * pd)
{
    ec_master_state_t state;
    master_device * dev = (master_device *)base;
    ecrt_master_state(base->master, &state);
    dev->slaves_responding = state.slaves_responding;
    dev->al_states = state.al_states;
    dev->link_up = state.link_up;
    dev->cycle++;
}

ethercat_device * init_master_device(ec_master_t * master)
{
    int n;
    master_device * dev = calloc(1, sizeof(master_device));
    dev->base.process = process_master;
    dev->base.master = master;

    enum { N_DEFAULT = 4 };
    dev->base.n_fields = N_DEFAULT;
    dev->base.fields = calloc(dev->base.n_fields, sizeof(field_t));
    field_t default_fields[N_DEFAULT] =
    {
        { "slaves", &dev->slaves_responding, sizeof(dev->slaves_responding) },
        { "alarm",  &dev->al_states,         sizeof(dev->al_states)         },
        { "link",   &dev->link_up,           sizeof(dev->link_up)           },
        { "cycle",  &dev->cycle,             sizeof(dev->cycle)             }
    };        

    for(n = 0; n < N_DEFAULT; n++)
    {    
        dev->base.fields[n].name = default_fields[n].name;
        dev->base.fields[n].buffer = default_fields[n].buffer;
        dev->base.fields[n].size = default_fields[n].size;
    }

    return (ethercat_device *)dev;
};

field_t * find_field(ethercat_device_config * chain,
                     ec_addr * loc)
{
    /* looks like dbNameToAddr */
    ethercat_device_config * c;
    for(c = chain; c != NULL; c = c->next)
    {
        if(c->vaddr == loc->vaddr)
        {
            int n;
            ethercat_device * d = c->dev;
            for(n = 0; n < d->n_fields; n++)
            {
                if(strcmp(d->fields[n].name, loc->name) == 0)
                {
                    return &d->fields[n];
                }
            }
        }
    }
    return NULL;
}

void process_slave_metadata(ethercat_device * d, uint8_t * pd)
{
    ecrt_master_get_slave(d->master, d->pos, &d->slave_info);
    d->al_state = d->slave_info.al_state;
    d->error_flag = d->slave_info.error_flag;
}

ethercat_device * clone_from_prototype(ethercat_device * proto, 
                                       ethercat_device_config * conf,
                                       ec_master_t * master,
                                       ec_domain_t * domain)
{
    int n = 0;
    printf("device init at pos %d\n", conf->pos);
    pdo_entry_type * r;
    ethercat_device * dev = calloc(1, sizeof(ethercat_device));
    assert(dev);
    dev->master = master;
    dev->regs = proto->regs;
    dev->pos = conf->pos;
    dev->usr = conf->usr;
    dev->dcactivate = proto->dcactivate;

    if(sscanf(dev->usr, "OS%d", &dev->oversample) == 1)
    {
        printf("oversampling factor is %d\n", dev->oversample);
    }
    
    dev->process = process_slave_metadata;

    for(r = dev->regs; r != NULL; r = r->next)
    {
        printf("found PDO name %s\n", r->name);
    }

    // alias not supported by some ecrt_ functions
    int alias = 0;
    ec_slave_config_t * sc = ecrt_master_slave_config(
        master, alias, conf->pos, proto->slave_info.vendor_id, 
        proto->slave_info.product_code);
    assert(sc);

    for(r = dev->regs; r != NULL; r = r->next)
    {
        int s;
        if(r->length > 1)
        {
            printf("adding extra PDO entries for %s(%d)\n", 
                   r->name, r->entry->bit_length);
            assert(dev->oversample <= r->length);
            r->length = dev->oversample;
        }
        for(s = 1; s < r->length; s++)
        {
            assert(ecrt_slave_config_pdo_mapping_add(
                       sc, r->pdo->index, 
                       r->entry->index + s * r->entry->bit_length, 
                       r->entry->subindex, r->entry->bit_length) == 0);
        }
    }

    assert(ecrt_master_get_slave(master, conf->pos, &dev->slave_info) == 0);
    printf("cloning %s\n", dev->slave_info.name);
    
    /* register the PDO entries and allocate a local buffer */
    int buffer_size = 0;
    int nregs = 0;

    for(r = dev->regs; r != NULL; r = r->next)
    {
        int s;

        printf("%d\n", r->length);

        r->pdo_offset = calloc(r->length, sizeof(unsigned int));
        r->pdo_bit = calloc(r->length, sizeof(unsigned int));
        
        r->buffer_offset = buffer_size;

        for(s = 0; s < r->length; s++)
        { 
            printf("%08x %04x\n", 
                   r->entry->index + s * r->entry->bit_length, 
                   r->entry->subindex);
            r->pdo_offset[s] = ecrt_slave_config_reg_pdo_entry(
                sc, r->entry->index + s * r->entry->bit_length, 
                r->entry->subindex, domain, &r->pdo_bit[s]);
        }

        assert((r->entry->bit_length == 1 && r->length == 1) ||
               r->entry->bit_length == 8  || 
               r->entry->bit_length == 16 || 
               r->entry->bit_length == 32);

        /* only support int32 interface */
        r->buffer_size = sizeof(int32_t) * r->length;

        buffer_size += r->buffer_size;
        nregs++;
    }
    dev->pdo_buffer = calloc(buffer_size, sizeof(char));
    printf("allocated %d bytes\n", buffer_size);

    /* attach the run-time type information */
    enum { N_DEFAULT = 2 };
    field_t default_fields[N_DEFAULT] =
    {
        {"al_state", &dev->al_state, 
         sizeof(dev->al_state) },
        {"error_flag", &dev->error_flag, 
         sizeof(dev->error_flag) },
    };
    dev->n_fields = nregs + N_DEFAULT;
    dev->fields = calloc(dev->n_fields, sizeof(field_t));
    for(n = 0, r = dev->regs; r != NULL; r = r->next, n++)
    {
        dev->fields[n].name = r->name;
        dev->fields[n].buffer = dev->pdo_buffer + r->buffer_offset;
        dev->fields[n].size = r->buffer_size;
    }
    for(n = 0; n < N_DEFAULT; n++)
    {
        dev->fields[nregs + n].name = default_fields[n].name;
        dev->fields[nregs + n].buffer = default_fields[n].buffer;
        dev->fields[nregs + n].size = default_fields[n].size;
    }

    if(dev->oversample > 0)
    {
        printf("distributed clocks %d %08x\n", dev->oversample, dev->dcactivate);
        ecrt_slave_config_dc(
            sc, dev->dcactivate, PERIOD_NS / dev->oversample,
            0, PERIOD_NS, 0);
    }


    printf("\n");

    return dev;
}

void initialize_chain(ethercat_device_config * chain, ethercat_device * devices, 
                      ec_master_t * master, ec_domain_t * domain)
{
    ethercat_device_config * c;
    for(c = chain; c != NULL; c = c->next)
    {
        printf("%s %d %d %s\n", c->name, c->vaddr, c->pos, c->usr);
        ethercat_device * d;

        if(strcmp(c->name, "MASTER") == 0)
        {
            printf("found MASTER\n");
            c->dev = init_master_device(master);
        }
        else
        {
            for(d = devices; d != NULL; d = d->next)
            {
                if(strcmp(d->name, c->name) == 0)
                {
                    printf("found match %s %08x\n", 
                           d->name, d->slave_info.product_code);
                    c->dev = clone_from_prototype(d, c, master, domain);
                    break;
                }
            }
        }
        if(c->dev == NULL)
        {
            printf("error: no device description found in config.xml for chain device %s\n", 
                   c->name);
            exit(1);
        }
    }
}
