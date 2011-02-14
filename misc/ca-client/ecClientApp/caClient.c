#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <unistd.h>
#include <stdint.h>

#include "cadef.h"

char *pv = "ECTESTOUT";

int main(int argc,char **argv)
{
    long int data;
    chid     mychid;
    int      n = 0;
    int      period = 100;

    if (argc < 2)
    {
        printf("Usage: %s <pvname>\n", argv[0]);
        return 1;   
    }
    pv = argv[1];
    SEVCHK(ca_context_create(ca_disable_preemptive_callback),"ca_context_create");
    SEVCHK(ca_create_channel(pv,NULL,NULL,10,&mychid),"ca_create_channel failure");
    SEVCHK(ca_pend_io(5.0),"ca_pend_io failure");

    while(1)
    {
        n++;
        data = (long int)((1 + sin(2 * M_PI * n / period)) * 8000);
        SEVCHK(ca_put(DBR_LONG,mychid,(void *)&data),"ca_put failure");
        SEVCHK(ca_pend_io(5.0),"ca_pend_io failure");
        if (n % 1000 == 0) printf("%ld\n",(long int)data);
        usleep(1000);
    }

    return(0);
}
