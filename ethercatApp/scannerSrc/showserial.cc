#include <stdio.h>
#include <ecrt.h>
#include <map>

// test program mapping serial numbers to bus position

int main(int argc, char ** argv)
{
    unsigned int n;
    ec_master_info_t minfo;
    ec_master_t * master = ecrt_request_master(0);
    std::map<unsigned int, unsigned int> serials;
    ecrt_master(master, &minfo);
    for(n = 0; n < minfo.slave_count; n++)
    {
        ec_slave_info_t sinfo;
        ecrt_master_get_slave(master, n, &sinfo);
        printf("slave %d serial %d\n", n, sinfo.serial_number);
        if(serials.find(sinfo.serial_number) != serials.end())
        {
            // we have a duplicate
        }
        serials[sinfo.serial_number] = n;
    }
    return 0;
}
