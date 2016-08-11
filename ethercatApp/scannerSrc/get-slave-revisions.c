#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <assert.h>

// IOCTL API exposes some features not present in the C API
#include "ioctl.h"

char * usage =
    "Usage: %s master_no\n"
    "EtherCAT Serial Number Display Tool\n\n"
    "  master_no - Master number, e.g. 0, 1 or 2\n";

#define DEVNAMELEN 80

int read_slave_revision()
{
    char devname[DEVNAMELEN];
    int fd;
    int master_no = 0;
    int master_count;
    int index;
    
    sprintf(devname, "/dev/EtherCAT%d", master_no);
    fd = open(devname, O_RDONLY);

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
    master_count = module.master_count;
    
    printf("Master No, Slave_no,DCS_number,Revision Number\n");
    assert(close(fd) == 0);
    
    for (index = 0; index < master_count; ++index)
    {
    
        sprintf(devname, "/dev/EtherCAT%d", index);
        fd = open(devname, O_RDONLY);
        if(fd < 0)
        {
            perror("open");
            fprintf(stderr, "can't open device %s for reading\n", devname);
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
        uint16_t sii_words;
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
            sii.words = &sii_words;
            if(ioctl(fd, EC_IOCTL_SLAVE_SII_READ, &sii) < 0)
            {
                perror("EC_IOCTL_SLAVE_SII_READ");
                exit(1);
            }
            uint32_t * serial = (uint32_t *)sii.words;
            printf("%d,%d,DCS%08d,0x%08x\n",index, n,serial[0], slave.revision_number);
        }
    }
    return 0;
}

int main(int argc, char ** argv)
{
    read_slave_revision();
    return 0;
}
