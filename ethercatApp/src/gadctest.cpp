#include <stdio.h>
#include <stdint.h>
#include <asynPortDriver.h>
#include "gadc.h"

int main()
{
    int NUM_PARAMS = gadc_get_num_parameters() * 4;
    asynPortDriver * parent = new asynPortDriver("test", 1, NUM_PARAMS, asynInt32Mask, 0, 0, 1, 0, 0);
    gadc_t * adc[4];
    for(int n = 0; n < 4; n++)
    {
        adc[n] = gadc_new(parent, n);
        /*
        gadc_writeInt32_name(adc[n], "MODE", GADC_MODE_TRIGGERED);
        gadc_writeInt32_name(adc[n], "SAMPLES", 100000);
        gadc_writeInt32_name(adc[n], "CHANBUFF", 1000000);
        gadc_writeInt32_name(adc[n], "RETRIGGER", 1);
        gadc_writeInt32_name(adc[n], "INFO", 1);
        */
    }
    return 0;
}
