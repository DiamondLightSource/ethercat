#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

// IOCTL API exposes some features not present in the C API
#include "ioctl.h"

enum { READ_ACTION = 0, WRITE_ACTION = 1 };

char * usage = 
    "Usage: %s [OPTION]...\n"
    "EtherCAT Serial Number Display Tool\n\n"
    "-w BASE      write increasing serial numbers starting from BASE\n"
    "-p POS       program one slave at POS (default is all slaves after coupler)\n";

int writeserial(int action, int base, int pos)
{
    char * devname = "/dev/EtherCAT0";
    int fd;
    if(action == WRITE_ACTION)
    {
        fd = open(devname, O_RDWR);
    }
    else
    {
        fd = open(devname, O_RDONLY);   
    }
    if(fd < 0)
    {
        perror("open");
        fprintf(stderr, "can't open device %s for reading\n", devname);
    }
    
    ec_ioctl_module_t module;
    if(ioctl(fd, EC_IOCTL_MODULE, &module) < 0) 
    {
        perror("EC_IOCTL_MODULE");
        exit(1);
    }
    if(module.ioctl_version_magic != EC_IOCTL_VERSION_MAGIC)
    {
        fprintf(stderr, "version mismatch between /dev/EtherCAT (0x%x) and userspace tool (0x%x)\n",
                module.ioctl_version_magic, 
                EC_IOCTL_VERSION_MAGIC
            );
        exit(1);
    }

    ec_ioctl_master_t master;
    if(ioctl(fd, EC_IOCTL_MASTER, &master) < 0)
    {
        perror("EC_IOCTL_MASTER");
        exit(1);
    }

    ec_ioctl_slave_t slave;
    ec_ioctl_slave_sii_t sii;
    int n;
    for(n = 0; n < master.slave_count; n++)
    {
        slave.position = n;

        if(ioctl(fd, EC_IOCTL_SLAVE, &slave) < 0)
        {
            perror("EC_IOCTL_SLAVE");
            exit(1);
        }
        sii.slave_position = n;
        sii.offset = 0xe;
        sii.nwords = 2; //slave.sii_nwords;
        sii.words = calloc(sii.nwords, sizeof(uint16_t));
        if(ioctl(fd, EC_IOCTL_SLAVE_SII_READ, &sii) < 0)
        {
            perror("EC_IOCTL_SLAVE_SII_READ");
            exit(1);
        }

        printf("slave:   %d\n", n);
        printf("name:    %s\n", slave.name);
        printf("vendor:  0x%08x\n", slave.vendor_id);
        printf("product: 0x%08x\n", slave.product_code);
        printf("revision: 0x%08x\n", slave.revision_number);
        uint32_t * serial = (uint32_t *)sii.words;
        printf("serial:  %d\n", serial[0]);
        
        if(((n == pos) || (pos == -1 && n > 0)) && (action == WRITE_ACTION))
        {
            if(pos == -1)
            {
                *serial = base + n - 1;
            }
            else
            {
                *serial = base;
            }
            if(ioctl(fd, EC_IOCTL_SLAVE_SII_WRITE, &sii) < 0)
            {
                perror("EC_IOCTL_SLAVE_SII_WRITE");
                exit(1);
            }
            printf("WRITE:   %d\n", *serial);
        }
        printf("\n");
        
        free(sii.words);
    }
    
    return 0;
}

int main(int argc, char ** argv)
{
    int pos = -1;
    int base = 0;
    int action = READ_ACTION;
    while(1)
    {
        int c = getopt(argc, argv, "w:p:");
        if(c == -1)
        {
            break;
        }
        switch(c)
        {
        case 'p':
            pos = atoi(optarg);
            break;
        case 'w':
            action = WRITE_ACTION;
            base = atoi(optarg);
            break;
        default:
            fprintf(stderr, usage, argv[0]);
            exit(1);
        }
    }
    if(optind < argc)
    {
        fprintf(stderr, usage, argv[0]);
        exit(1);
    }
    writeserial(action, base, pos);
    return 0;
}
