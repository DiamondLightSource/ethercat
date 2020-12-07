#include <stdio.h>
#include <string.h>
#include <ecrt.h>
#include <unistd.h>

#include "slave-types.h"
#include "slave-list-path.h"
#include "version.h"

typedef struct
{
    int master_index;
    ec_master_t *master;
    ec_slave_info_t **slave_info;
    char **slave_names;
    int *getslaveresult;
    int *checkvalid;
    ec_master_info_t master_info;
    int all_slaves_valid;
} businfo_t;

#define SCANBUS_OK 0
#define SCANBUS_ERROR 1
/* return OK or ERROR */

int scanbus(businfo_t *i)
{
    int result;
    ec_slave_info_t *curr_slave;
    int n;
    int count;
    i->all_slaves_valid = YES;
    i->master = ecrt_open_master(i->master_index);
    if(i->master == NULL) {
        fprintf(stderr, "can't get ethercat master %d\n",
                i->master_index);
        return(SCANBUS_ERROR);
    }
    result = ecrt_master(i->master, &i->master_info);
    if (result != 0)
    {
        fprintf(stderr, "can't get master info\n");
        return(SCANBUS_ERROR);
    }
    count = i->master_info.slave_count;
    i->slave_info     = calloc(count, sizeof(ec_slave_info_t *));
    i->slave_names    = calloc(count, sizeof(char *));
    i->getslaveresult = calloc(count, sizeof(int));
    i->checkvalid     = calloc(count, sizeof(int));
    for(n = 0; n < count; n++)
    {
        curr_slave = calloc(1, sizeof(ec_slave_info_t));
        i->slave_info[n] = curr_slave;
        i->getslaveresult[n] = ecrt_master_get_slave(i->master, n,
                                       curr_slave);
        i->checkvalid[n] = check_valid_slave(
            curr_slave->name, curr_slave->revision_number);
        if (i->checkvalid[n] == NO)
            i->all_slaves_valid = NO;
        if (i->getslaveresult[n] == 0)
        {
            i->slave_names[n] = shorten_name(curr_slave->name);
        }
        else
        {
            fprintf(stderr, "can't get info for slave %d\n", n);
            i->slave_names[n] = "";
        }
    }
    return(SCANBUS_OK);
}

/* bus information in parameter i, whether to write dcs numbers in dcs
 * parameter */
void writexml(businfo_t *i, int dcs)
{
    int n;
    printf("<components arch=\"linux-x86_64\">\n");
    printf("  <ethercat.EthercatMaster name=\"ECATM\""
           " socket=\"/tmp/socket%d\" master_index=\"%d\"/>\n",
           i->master_index, i->master_index);
    printf("  <!-- slaves %d link up %d -->\n",
           i->master_info.slave_count, i->master_info.link_up);
    if (i->all_slaves_valid == NO)
    {
        printf("  <!-- WARNING: bus contains slaves not supported by "
               "the EPICS module at release %s-->\n", VERSION_STRING);
        printf("  <!-- WARNING: Please attempt upgrade or contact "
               "module maintainer -->\n");
    }
    for(n = 0; n < i->master_info.slave_count; n++)
    {
        ec_slave_info_t *curr_slave = i->slave_info[n];
        if ((i->all_slaves_valid == NO) && (i->checkvalid[n] == NO))
        {
            printf("  <!-- Slave not supported -->\n");
        }
        printf("  <ethercat.EthercatSlave master=\"ECATM\" name=\"ERIO.%d\" ", n);
        if (dcs) {
            printf("position=\"DCS%08d\" ", curr_slave->serial_number);
        } else {
            printf("position=\"%d\" ", n);
        }
        printf("type_rev=\"%s rev 0x%08x\"/>\n", i->slave_names[n],
               curr_slave->revision_number);
    }
    printf("</components>\n");
}

const char *usage = "Usage: slaveinfo [options]\n"
"\n"
"This helper script scans the ethercat bus and prints an xml file of the\n"
"slaves on the bus.\n"
"\n"
"Options:\n"
"  -h                show this help message and exit\n"
"  -d                write dcs serial numbers instead of positional info\n"
"  -m <master_index> master to use (defaults to 0)\n"
"  -s <slave-list>   use slave-list file";


int main(int argc, char ** argv) {
    int dcs=0;
    businfo_t thebus;
    thebus.master_index = 0;
    opterr = 0;
    while (1)
    {
        int cmd = getopt (argc, argv, "hdm:s:");
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
            thebus.master_index = atoi(optarg);
            break;
        case 's':
            if (set_slave_list(optarg) != YES)
            {
                printf("Error: could not set slave list file\n");
                exit(1);
            }
            break;
        }
    }

    if(argc - optind > 0)
    {
        printf(usage);
        exit(1);
    }

    char *slave_list_filename = get_slave_list_filename(argv[0]);
    read_valid_slaves(slave_list_filename);
    free(slave_list_filename);

    if (scanbus(&thebus) != SCANBUS_OK)
    {
        exit(1);
    }
    writexml(&thebus, dcs);
    return 0;
}
