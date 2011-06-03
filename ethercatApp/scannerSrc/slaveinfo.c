#include <stdio.h>
#include <string.h>
#include <ecrt.h>

int main()
{
    ec_master_info_t master_info;
    ec_master_t * master = ecrt_request_master(0);
    if(master == NULL)
    {
        fprintf(stderr, "can't get ethercat master 0\n");
        exit(1);
    }
    if(ecrt_master(master, &master_info) != 0)
    {
        fprintf(stderr, "can't get master info\n");
        exit(1);
    }
    printf("<chain>\n");
    printf("  <!-- slaves %d link up %d -->\n", master_info.slave_count, master_info.link_up);

    int n;
    for(n = 0; n < master_info.slave_count; n++)
    {
        ec_slave_info_t slave_info;
        if(ecrt_master_get_slave(master, n, &slave_info) != 0)
        {
            fprintf(stderr, "can't get info for slave %d\n", n);
            continue;
        }
        char * name = strtok(slave_info.name, " ");
        if(name == NULL)
        {
            name = "NAME ERROR";
        }
        printf("  <device type_name=\"%s\" revision=\"0x%08x\" position=\"%d\" name=\"PORT%d\" />\n", 
               name, slave_info.revision_number, n, n);
    }
    printf("</chain>\n");
    return 0;
}
