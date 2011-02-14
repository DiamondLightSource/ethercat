#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stddef.h>
#include <ecrt.h>

#include "ethercat_device.h"
#include "EK1101.h"

/* Master 0, Slave 0, "EK1101"
 * Vendor ID:       0x00000002
 * Product code:    0x044d2c52
 * Revision number: 0x00110000
 */

ec_pdo_entry_info_t slave_0_pdo_entries[] = {
    {0x6000, 0x01, 16}, /* ID */
};

ec_pdo_info_t slave_0_pdos[] = {
    {0x1a00, 1, slave_0_pdo_entries + 0}, /* ID */
};

ec_sync_info_t slave_0_syncs[] = {
    {0, EC_DIR_INPUT, 1, slave_0_pdos + 0, EC_WD_DISABLE},
    {0xff}
};

void EK1101_process(ethercat_device * self, uint8_t * pd)
{
    EK1101_device * dev = (EK1101_device *)self;
    dev->regs.ID = EC_READ_U16(pd + dev->ID);
}

void EK1101_show(ethercat_device * self)
{
    printf("show: EK1101\n");
}

static field_t fields[] =
{
    { "ID", offsetof(EK1101_device, regs.ID), sizeof(uint16_t)  },
    { "alarm", offsetof(EK1101_device, self.alarm), sizeof(uint8_t)  },
    { NULL }
};

ethercat_device * EK1101_init(
    ec_master_t * master,
    ec_domain_t * domain,
    int alias, int pos, char * usr)
{
    EK1101_device * dev = calloc(1, sizeof(EK1101_device));
    dev->self.process = EK1101_process;
    dev->self.show = EK1101_show;
    dev->self.pos = pos;
    dev->self.alias = alias;
    dev->self.usr = usr;
    dev->self.fields = fields;
    dev->self.get_field = ethercat_device_get_field;
    dev->self.read_field = ethercat_device_read_field;

    assert(dev);
    ec_slave_config_t * sc = ecrt_master_slave_config(
        master, alias, pos, Beckhoff, EK1101);
    assert(sc);
    assert(ecrt_slave_config_pdos(
               sc, EC_MAX_SYNC_MANAGERS, slave_0_syncs) == 0);
    dev->ID = ecrt_slave_config_reg_pdo_entry(
        sc, 0x6000, 0x01, domain, NULL);
    assert(dev->ID >= 0);
    dev->self.vendor = Beckhoff;
    dev->self.product = EK1101;
    return (ethercat_device *)dev;
}
