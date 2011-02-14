#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <ecrt.h>

#include "ethercat_device.h"
#include "EL4102.h"

/* Master 0, Slave 1, "EL4102"
 * Vendor ID:       0x00000002
 * Product code:    0x10063052
 * Revision number: 0x03fa0000
 */

ec_pdo_entry_info_t slave_1_pdo_entries[] = {
    {0x3001, 0x01, 16}, /* Output */
    {0x3002, 0x01, 16}, /* Output */
};

ec_pdo_info_t slave_1_pdos[] = {
    {0x1600, 1, slave_1_pdo_entries + 0}, /* RxPDO 01 mapping */
    {0x1601, 1, slave_1_pdo_entries + 1}, /* RxPDO 02 mapping */
};

ec_sync_info_t slave_1_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 2, slave_1_pdos + 0, EC_WD_DISABLE},
    {3, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {0xff}
};

void EL4102_write(ethercat_device * self, int channel, uint64_t value)
{
    EL4102_device * dev = (EL4102_device *)self;
    dev->regs.output[channel] = value;
}

void EL4102_process(ethercat_device * self, uint8_t * pd)
{
    EL4102_device * dev = (EL4102_device *)self;
    EC_WRITE_U16(pd + dev->output[0], dev->regs.output[0]);
    EC_WRITE_U16(pd + dev->output[1], dev->regs.output[1]);
}

void EL4102_show(ethercat_device * self)
{
    printf("show: EL4102\n");
}

static field_t fields[] =
{
    { "output0", offsetof(EL4102_regs, output[0]), sizeof(uint16_t)  },
    { "output1", offsetof(EL4102_regs, output[1]), sizeof(uint16_t)  },
    { NULL }
};

ethercat_device * EL4102_init(
    ec_master_t * master,
    ec_domain_t * domain,
    int alias, int pos, char * usr)
{
    EL4102_device * dev = calloc(1, sizeof(EL4102_device));
    dev->self.process = EL4102_process;
    dev->self.write = EL4102_write;
    dev->self.show = EL4102_show;
    dev->self.pos = pos;
    dev->self.alias = alias;
    dev->self.usr = usr;

    dev->self.fields = fields;
    dev->self.get_field = ethercat_device_get_field;
    dev->self.read_field = ethercat_device_read_field;

    assert(dev);
    ec_slave_config_t * sc = ecrt_master_slave_config(
        master, alias, pos, Beckhoff, EL4102);
    assert(sc);
    assert(ecrt_slave_config_pdos(
               sc, EC_MAX_SYNC_MANAGERS, slave_1_syncs) == 0);
    dev->output[0] = ecrt_slave_config_reg_pdo_entry(
        sc, 0x3001, 0x01, domain, NULL);
    assert(dev->output[0] >= 0);
    dev->output[1] = ecrt_slave_config_reg_pdo_entry(
        sc, 0x3002, 0x01, domain, NULL);
    assert(dev->output[0] >= 0);
    dev->self.vendor = Beckhoff;
    dev->self.product = EL4102;
    return (ethercat_device *)dev;
}
