#include <stddef.h>
#include <stdint.h>
#include <ecrt.h>

#include "ethercat_device.h"
#include "master_device.h"

typedef struct
{
    ethercat_device base;
    ec_master_t * master;
    ec_master_state_t ms;
    uint16_t slaves_responding;
    uint8_t al_states;
    uint8_t link_up;
} master_device;

static field_t fields[] =
{
    { "slaves", offsetof(master_device, slaves_responding), sizeof(uint16_t) },
    { "alarm",  offsetof(master_device, al_states), sizeof(uint8_t) },
    { "link",   offsetof(master_device, link_up),  sizeof(uint8_t) },
    { NULL }
};

void master_device_process(ethercat_device * base, uint8_t * dummy)
{
    master_device * dev = (master_device *)base;
    ecrt_master_state(dev->master, &dev->ms);
    dev->slaves_responding = dev->ms.slaves_responding;
    dev->al_states = dev->ms.al_states;
    dev->link_up = dev->ms.link_up;
}

ethercat_device * master_device_init(
    ec_master_t * master,
    ec_domain_t * domain)
{
    master_device * dev = calloc(1, sizeof(master_device));
    dev->master = master;
    dev->base.process = master_device_process;
    dev->base.fields = fields;
    dev->base.get_field = ethercat_device_get_field;
    dev->base.read_field = ethercat_device_read_field;
    return (ethercat_device *)dev;
}
