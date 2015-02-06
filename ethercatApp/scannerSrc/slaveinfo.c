#include <stdio.h>
#include <string.h>
#include <ecrt.h>
#include <unistd.h>

const char *usage = "Usage: slaveinfo [options]\n"
"\n"
"This helper script scans the ethercat bus and prints an xml file of the\n"
"slaves on the bus.\n"
"\n"
"Options:\n"
"  -h                show this help message and exit\n"
"  -d                write dcs serial numbers instead of positional info\n"
"  -m <master_index> master to use (defaults to 0)\n";

int main(int argc, char ** argv) {
    int dcs=0;
    int master_index = 0;
    opterr = 0;
    while (1)
    {
        int cmd = getopt (argc, argv, "hdm:");
        if(cmd == -1)
        {
            break;
        }
        switch(cmd)
        {
        case 'h':
            printf(usage);
            exit(0);
            break;
        case 'd':
            dcs = 1;
            break;
        case 'm':
            master_index = atoi(optarg);
            break;
        }
    }
    
    if(argc - optind > 0)
    {
        printf(usage);
        exit(1);
    }
    
    ec_master_info_t master_info;
    ec_master_t * master = ecrt_open_master(master_index);
    if(master == NULL) {
        fprintf(stderr, "can't get ethercat master 0\n");
        exit(1);
    }
    if(ecrt_master(master, &master_info) != 0) {
        fprintf(stderr, "can't get master info\n");
        exit(1);
    }
    
    printf("<components arch=\"linux-x86_64\">\n");
    printf("  <ethercat.EthercatMaster name=\"ECATM\""
           " socket=\"/tmp/socket%d\" master_index=\"%d\"/>\n",
           master_index, master_index);
    printf("  <!-- slaves %d link up %d -->\n", master_info.slave_count, master_info.link_up);

    int n;
    for(n = 0; n < master_info.slave_count; n++) {
        ec_slave_info_t slave_info;
        if(ecrt_master_get_slave(master, n, &slave_info) != 0) {
            fprintf(stderr, "can't get info for slave %d\n", n);
            continue;
        }

#if 0        
        printf("position: %d\n", slave_info.position); /**< Offset of the slave in the ring. */
        printf("vendor_id: %d\n", slave_info.vendor_id); /**< Vendor-ID stored on the slave. */
        printf("product_code: %d\n", slave_info.product_code); /**< Product-Code stored on the slave. */
        printf("revision_number: %d\n", slave_info.revision_number); /**< Revision-Number stored on the slave. */
        printf("serial_number: %d\n", slave_info.serial_number); /**< Serial-Number stored on the slave. */
        printf("alias: %d\n", slave_info.alias); /**< The slaves alias if not equal to 0. */
        printf("current_on_ebus: %d\n", slave_info.current_on_ebus); /**< Used current in mA. */
        printf("al_state: %d\n", slave_info.al_state); /**< Current state of the slave. */
        printf("error_flag: %d\n", slave_info.error_flag); /**< Error flag for that slave. */
        printf("sync_count: %d\n", slave_info.sync_count); /**< Number of sync managers. */
        printf("sdo_count: %d\n", slave_info.sdo_count); /**< Number of SDOs. */
        printf("name: %s\n", slave_info.name); /**< Name of the slave. */
#endif

        /* slave.info name shows a whole name such as  */
        /* EL2024 4K. Dig. Ausgang 24V, 2A rev 0x00110000 */
        /* trim to the first space. */
        /* There's a special case for the national instruments backplane */
#define NI9144_NAME "NI 9144"
        char *name;
        if (strcmp(slave_info.name, NI9144_NAME) == 0)
          name = slave_info.name;
        else
          name = strtok(slave_info.name, " ");
        
        if(name == NULL) {
            name = "NAME ERROR";
        }
        printf("  <ethercat.EthercatSlave master=\"ECATM\" name=\"ERIO.%d\" ", n);
        if (dcs) {
            printf("position=\"DCS%08d\" ", slave_info.serial_number);
        } else {
            printf("position=\"%d\" ", n);
        }
        printf("type_rev=\"%s rev 0x%08x\"/>\n", name, slave_info.revision_number);
    }
    printf("</components>\n");
    return 0;
}
