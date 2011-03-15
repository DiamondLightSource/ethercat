/* 
   ethercat device XML dump (simple format)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <ecrt.h>

void show_device(ec_master_t * master, int pos, FILE * f);

int main(int argc, char **argv)
{
    ec_master_t * master = NULL;
    ec_domain_t * domain = NULL;

    master = ecrt_request_master(0);
    assert(master);

    domain = ecrt_master_create_domain(master);
    assert(domain);

    ec_master_state_t state;
    ecrt_master_state(master, &state);
    
    printf("<devices>\n");
    int pos;
    for(pos = 0; pos < state.slaves_responding; pos++)
    {
        show_device(master, pos, stdout);
    }
    printf("</devices>\n");

    return 0;

}

void show_device(ec_master_t * master, int pos, FILE * f)
{
    ec_sync_info_t syncs[EC_MAX_SYNC_MANAGERS];
    ec_slave_info_t slave_info;

    char next_name[1024] = {0};
    int names = 0;

    assert(ecrt_master_get_slave(master, pos, &slave_info) == 0);

    fprintf(f, "  <device name=\"%s\" vendor=\"0x%08x\" product=\"0x%08x\" revision=\"0x%08x\">\n",
            slave_info.name,
            slave_info.vendor_id,
            slave_info.product_code,
            slave_info.revision_number);

    int n;
    for(n = 0; n < slave_info.sync_count; n++)
    {
        ec_sync_info_t * sync = syncs + n;
        assert(ecrt_master_get_sync_manager(
                   master, pos, n, sync) == 0);
        ec_pdo_info_t * pdos = calloc(sync->n_pdos, sizeof(ec_pdo_info_t));
        sync->pdos = pdos;
        fprintf(f, "    <sync index=\"%d\" n_pdos=\"%d\" dir=\"%d\" watchdog=\"%d\">\n", 
                sync->index, sync->n_pdos, sync->dir, sync->watchdog_mode);

        int p;
        for(p = 0; p < sync->n_pdos; p++)
        {
            ec_pdo_info_t * pdo = pdos + p;
            assert(ecrt_master_get_pdo(
                       master, pos, n, p, pdo) == 0);
            ec_pdo_entry_info_t * entries = calloc(
                pdo->n_entries, sizeof(ec_pdo_entry_info_t));
            pdo->entries = entries;
            fprintf(f, "      <pdo index=\"0x%04x\" n_entries=\"%d\" >\n", pdo->index, pdo->n_entries);
            
            int j;
            for(j = 0; j < pdo->n_entries; j++)
            {
                ec_pdo_entry_info_t * entry = entries + j;
                assert(ecrt_master_get_pdo_entry(
                           master, pos, n, p, j, entry) == 0);

                snprintf(next_name, sizeof(next_name), "entry%d", names++);
                if(entry->index == 0 && entry->subindex == 0)
                {
                    // gap
                }
                else
                {
                    fprintf(f, "        <entry index=\"0x%04x\" subindex=\"0x%02x\" bit_length=\"%d\" name=\"%s\" />\n", 
                            entry->index, entry->subindex, entry->bit_length, next_name);
                }
            }
            fprintf(f, "      </pdo>\n");
        }
        fprintf(f, "    </sync>\n");
    }
    fprintf(f, "  </device>\n");
}
