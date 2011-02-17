#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <ecrt.h>

#include "ethercat_device.h"
#include "EL3702.h"

/* Master 0, Slave 2, "EL3702"
 * Vendor ID:       0x00000002
 * Product code:    0x0e763052
 * Revision number: 0x00020000
 */

ec_pdo_entry_info_t slave_2_pdo_entries[] = {
    {0x6800, 0x01, 16}, /* Ch1 CycleCount */
    {0x6000, 0x01, 16}, /* Ch1 Value */
    {0x6800, 0x02, 16}, /* Ch2 CycleCount */
    {0x6000, 0x02, 16}, /* Ch2 Value */
    {0x1d09, 0x98, 32}, /* StartTimeNextLatch */
};

ec_pdo_info_t slave_2_pdos[] = {
    {0x1b00, 1, slave_2_pdo_entries + 0}, /* Ch1 CycleCount */
    {0x1a00, 1, slave_2_pdo_entries + 1}, /* Ch1 Sample */
    {0x1b01, 1, slave_2_pdo_entries + 2}, /* Ch2 CycleCount */
    {0x1a80, 1, slave_2_pdo_entries + 3}, /* Ch2 Sample */
    {0x1b10, 1, slave_2_pdo_entries + 4}, /* NextSync1Time */
};

ec_sync_info_t slave_2_syncs[] = {
    {0, EC_DIR_INPUT, 2, slave_2_pdos + 0, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 2, slave_2_pdos + 2, EC_WD_DISABLE},
    {2, EC_DIR_INPUT, 1, slave_2_pdos + 4, EC_WD_DISABLE},
    {0xff}
};

void EL3702_process(ethercat_device * self, uint8_t * pd)
{
    int s;
    EL3702_device * dev = (EL3702_device *)self;
    for(s = 0; s < dev->factor; s++)
    {
        dev->regs.value[0][s] = EC_READ_S16(pd + dev->value[0][s]);
        dev->regs.value[1][s] = EC_READ_S16(pd + dev->value[1][s]);
        dev->regs.cycle[0] = EC_READ_U16(pd + dev->cycle[0]);
        dev->regs.cycle[1] = EC_READ_U16(pd + dev->cycle[1]);
    }
}

void EL3702_show(ethercat_device * self)
{
    EL3702_device * dev = (EL3702_device *)self;
    printf("show: EL3702 (oversampling factor %d)\n", dev->factor);
}

static field_t fields[] =
{
    { "cycle0", offsetof(EL3702_device, regs.cycle[0]), sizeof(uint16_t)  },
    { "cycle1", offsetof(EL3702_device, regs.cycle[1]), sizeof(uint16_t)  },
    { "value0", offsetof(EL3702_device, regs.value[0][0]), 100*sizeof(int16_t) },
    { "value1", offsetof(EL3702_device, regs.value[1][0]), 100*sizeof(int16_t) },
    { "alarm",  offsetof(EL3702_device, self.alarm), sizeof(uint8_t) },
    { NULL }
};

ethercat_device * EL3702_init(
    ec_master_t * master,
    ec_domain_t * domain,
    int alias, int pos, char * usr)
{
    EL3702_device * dev = calloc(1, sizeof(EL3702_device));
    dev->self.process = EL3702_process;
    dev->self.show = EL3702_show;
    dev->self.pos = pos;
    dev->self.alias = alias;
    dev->self.usr = usr;
    
    dev->factor = atoi(usr);

    dev->self.fields = fields;
    dev->self.get_field = ethercat_device_get_field;
    dev->self.read_field = ethercat_device_read_field;

    // adjust size of waveforms
    dev->self.fields[2].size = dev->factor * sizeof(int16_t);
    dev->self.fields[3].size = dev->factor * sizeof(int16_t);

    assert(dev);
    ec_slave_config_t * sc = ecrt_master_slave_config(
        master, alias, pos, Beckhoff, EL3702);
    assert(sc);
    assert(ecrt_slave_config_pdos(
               sc, EC_MAX_SYNC_MANAGERS, slave_2_syncs) == 0);

    int s;

    for(s = 1; s < dev->factor; s++)
    {
        assert(ecrt_slave_config_pdo_mapping_add(
                   sc, 0x1a00, 0x6000 + s * 0x0010, 0x01, 16) == 0);
        assert(ecrt_slave_config_pdo_mapping_add(
                   sc, 0x1a80, 0x6000 + s * 0x0010, 0x02, 16) == 0);
    }

    for(s = 0; s < dev->factor; s++)
    {
        dev->value[0][s] = ecrt_slave_config_reg_pdo_entry(
            sc, 0x6000 + s * 0x0010, 0x01, domain, NULL);
        dev->value[1][s] = ecrt_slave_config_reg_pdo_entry(
            sc, 0x6000 + s * 0x0010, 0x02, domain, NULL);
    }
    
    dev->cycle[0] = ecrt_slave_config_reg_pdo_entry(
        sc, 0x6800, 0x01, domain, NULL);

    dev->cycle[1] = ecrt_slave_config_reg_pdo_entry(
        sc, 0x6800, 0x02, domain, NULL);

    ecrt_slave_config_dc(
        sc, 0x0730, PERIOD_NS / dev->factor,
        0, PERIOD_NS, 0);

    dev->self.vendor = Beckhoff;
    dev->self.product = EL3702;
    return (ethercat_device *)dev;
}
