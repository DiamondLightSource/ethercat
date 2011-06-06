#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

// IOCTL API exposes some features not present in the C API
#include "ioctl.h"

char * usage = "usage: %s --read|(--write base_serial_decimal)\n";

int writeserial(int dowrite, int base)
{
    char * devname = "/dev/EtherCAT0";
    int fd;
    if(dowrite)
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
        uint32_t * serial = (uint32_t *)sii.words;
        printf("serial:  %d\n", serial[0]);
        printf("\n");

        *serial = base + n;
        
        if(dowrite)
        {
            if(ioctl(fd, EC_IOCTL_SLAVE_SII_WRITE, &sii) < 0)
            {
                perror("EC_IOCTL_SLAVE_SII_WRITE");
                exit(1);
            }
        }
        
        free(sii.words);
    }
    
    return 0;
}

int main(int argc, char ** argv)
{
    if(argc == 2 && strcmp(argv[1], "--read") == 0)
    {
        writeserial(0, 0);
    }
    else if(argc == 3 && strcmp(argv[1], "--write") == 0)
    {
        writeserial(1, atoi(argv[2]));
    }
    else
    {
        fprintf(stderr, usage, argv[0]);
        exit(1);
    }
    return 0;
}
