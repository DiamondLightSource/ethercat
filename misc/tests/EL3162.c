#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <ecrt.h>

#include "ethercat_device.h"
#include "EL3162.h"

/* Master 0, Slave 3, "EL3162"
 * Vendor ID:       0x00000002
 * Product code:    0x0c5a3052
 * Revision number: 0x00000000
 */

ec_pdo_entry_info_t slave_3_pdo_entries[] = {
    {0x3101, 0x01, 8}, /* Status */
    {0x3101, 0x02, 16}, /* Value */
    {0x3102, 0x01, 8}, /* Status */
    {0x3102, 0x02, 16}, /* Value */
};

ec_pdo_info_t slave_3_pdos[] = {
    {0x1a00, 2, slave_3_pdo_entries + 0}, /* TxPDO 001 mapping */
    {0x1a01, 2, slave_3_pdo_entries + 2}, /* TxPDO 002 mapping */
};

ec_sync_info_t slave_3_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {2, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
    {3, EC_DIR_INPUT, 2, slave_3_pdos + 0, EC_WD_DISABLE},
    {0xff}
};

static field_t fields[] =
{
    { "status0", offsetof(EL3162_device, regs.status[0]), sizeof(uint8_t)  },
    { "status1", offsetof(EL3162_device, regs.status[1]), sizeof(uint8_t)  },
    { "value0",  offsetof(EL3162_device, regs.value[0]),  sizeof(uint16_t) },
    { "value1",  offsetof(EL3162_device, regs.value[1]),  sizeof(uint16_t) },
    { "alarm",   offsetof(EL3162_device, self.alarm),     sizeof(uint8_t) },
    { NULL }
};

void EL3162_process(ethercat_device * self, uint8_t * pd)
{
    EL3162_device * priv = (EL3162_device *)self;
    priv->regs.status[0] = EC_READ_U8(pd + priv->status[0]);
    priv->regs.status[1] = EC_READ_U8(pd + priv->status[1]);
    priv->regs.value[0] = EC_READ_U16(pd + priv->value[0]);
    priv->regs.value[1] = EC_READ_U16(pd + priv->value[1]);
    // get alarm state here?
}

void EL3162_show(ethercat_device * self)
{
    printf("show: EL3162\n");
}

ethercat_device * EL3162_init(
    ec_master_t * master,
    ec_domain_t * domain,
    int alias, int pos, char * usr)
{
    EL3162_device * dev = calloc(1, sizeof(EL3162_device));
    dev->self.process = EL3162_process;
    dev->self.get_field = ethercat_device_get_field;
    dev->self.read_field = ethercat_device_read_field;
    dev->self.fields = fields;
    dev->self.show = EL3162_show;
    dev->self.pos = pos;
    dev->self.alias = alias;
    dev->self.usr = usr;

    assert(dev);
    ec_slave_config_t * sc = ecrt_master_slave_config(
        master, alias, pos, Beckhoff, EL3162);
    assert(sc);
    assert(ecrt_slave_config_pdos(
               sc, EC_MAX_SYNC_MANAGERS, slave_3_syncs) == 0);
    dev->status[0] = ecrt_slave_config_reg_pdo_entry(
        sc, 0x3101, 0x01, domain, NULL);
    assert(dev->status >= 0);
    dev->value[0] = ecrt_slave_config_reg_pdo_entry(
        sc, 0x3101, 0x02, domain, NULL);
    assert(dev->status >= 0);
    dev->status[1] = ecrt_slave_config_reg_pdo_entry(
        sc, 0x3102, 0x01, domain, NULL);
    assert(dev->status >= 0);
    dev->value[1] = ecrt_slave_config_reg_pdo_entry(
        sc, 0x3102, 0x02, domain, NULL);
    assert(dev->status >= 0);
    dev->self.vendor = Beckhoff;
    dev->self.product = EL3162;
    return (ethercat_device *)dev;
}
