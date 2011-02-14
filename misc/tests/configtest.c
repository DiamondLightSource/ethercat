#include <stdio.h>
#include <assert.h>
#include <ecrt.h>

int cstruct(ec_master_t * master, int pos);

int main(int argc, char **argv)
{
    ec_master_t * master = NULL;
    ec_domain_t * domain = NULL;

    master = ecrt_request_master(0);
    assert(master);

    domain = ecrt_master_create_domain(master);
    assert(domain);

    int pos;

    for(pos = 0; pos < 4; pos++)
    {
        cstruct(master, pos);
        printf("\n");
    }

    return 0;

}

int cstruct(ec_master_t * master, int pos)
{

    ec_slave_info_t slave_info;
    assert(ecrt_master_get_slave(master, pos, &slave_info) == 0);
    printf("'%s' 0x%x 0x%x %d\n", 
           slave_info.name, slave_info.vendor_id, slave_info.product_code, 
           slave_info.sync_count);

    ec_sync_info_t syncs[EC_MAX_SYNC_MANAGERS];
    
    int pp;
    int nsync;
    int j;

    for(nsync = 0; nsync < slave_info.sync_count; nsync++)
    {
        assert(ecrt_master_get_sync_manager(
                   master, pos, nsync, &syncs[nsync]) == 0);
        
        printf("sync 0x%x 0x%x 0x%x\n", 
               syncs[nsync].index, syncs[nsync].dir, syncs[nsync].watchdog_mode);

        for(pp = 0; pp < syncs[nsync].n_pdos; pp++)
        {
            ec_pdo_info_t pdo;
            
            assert(ecrt_master_get_pdo(
                       master, pos, nsync, pp, &pdo) == 0);
            
            printf("pdo 0x%0x\n", pdo.index);
            
            for(j = 0; j < pdo.n_entries; j++)
            {
                ec_pdo_entry_info_t entry;
                assert(ecrt_master_get_pdo_entry(master, pos, nsync, pp, j, &entry) == 0);
                
                printf("entry 0x%x 0x%x %d\n", entry.index, entry.subindex, entry.bit_length);
            }
        }
    }

    return 0;

}
