/*

Generic ADC driver
TDI-CTRL-REQ-015

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <asynPortDriver.h>
#include "gadc.h"

static int _max(int a, int b)
{
    return a > b ? a : b;
}

static int _min(int a, int b)
{
    return a < b ? a : b;
}

static int posmod(int a, int b)
{
    int c = a % b;
    while(c < 0)
    {
        c += b;
    }
    return c;
}

// note possible enhancement for asynPortDriver, setter function pointer table
asynStatus WaveformPort::writeInt32(asynUser * pasynUser, epicsInt32 value)
{
    asynStatus result = asynPortDriver::writeInt32(pasynUser, value);
    if(result != asynSuccess)
    {
        return result;
    }
    int cmd = pasynUser->reason;
    if(cmd == P_Samples)
    {
        return setSamples(value);
    }
    else if(cmd == P_Offset)
    {
        return setOffset(value);
    }
    else if(cmd == P_Chanbuff)
    {
        return setChanbuff(value);
    }
    else if(cmd == P_Trigger)
    {
        return setTrigger(value);
    }
    else if(cmd == P_Enabled)
    {
        return setEnabled(value);
    }
    else if(cmd == P_Clear)
    {
        return setClear(value);
    }
    else if(cmd == P_Info)
    {
        return setInfo(value);
    }
    else if(cmd == P_Interrupt)
    {
        return setInterrupt(value);
    }
    else if(cmd == P_Putsample)
    {
        return setPutsample(value);
    }
    return asynSuccess;
}

asynStatus WaveformPort::resize()
{
    free(buffer);
    free(outbuffer);
    bsize = 0;
    bofs = 0;
    int size = _max(IP(P_Samples), -IP(P_Offset));
    printf("resize %d\n", size);
    if(size > IP(P_Chanbuff))
    {
        printf("can't resize %d %d\n", size, IP(P_Chanbuff));
        return asynError;
    }
    buffer = (epicsInt32 *)calloc(size, sizeof(epicsInt32));
    if(buffer == NULL)
    {
        return asynError;
    }
    outbuffer = (epicsInt32 *)calloc(size, sizeof(epicsInt32));
    if(outbuffer == NULL)
    {
        return asynError;
    }
    bsize = size;
    return asynSuccess;
}

asynStatus WaveformPort::setSamples(epicsInt32 samples)
{
    if(samples < 0)
    {
        return asynError;
    }
    else
    {
        return resize();
    }
}

/*
static asynStatus gadc_set_offset(gadc_t * adc, epicsInt32 offset)
{
    if(INT(P_Offset) != offset)
    {
        return gadc_resize(adc);
    }
    else
    {
        return asynSuccess;
    }
}
*/

asynStatus WaveformPort::setChanbuff(epicsInt32 chanBuff)
{
    return resize();
}

/*
static void gadc_state_transition(gadc_t * adc, int event)
{
    if(event == GADC_EVENT_RESET)
    {
        adc->parent->setIntegerParam(adc->P_State, GADC_STATE_WAITING);
        adc->readcount = 0;
        adc->samplecount = 0;
    }
    else if(INT(P_State) == GADC_STATE_WAITING &&
            INT(P_Enabled) && 
            (event == GADC_EVENT_TRIGGER))
    {
        adc->parent->setIntegerParam(adc->P_State, GADC_STATE_TRIGGERED);
    }
    else if(INT(P_State) == GADC_STATE_TRIGGERED &&
            (event == GADC_EVENT_DONE))
    {
        gadc_set_enabled(adc, 0);
        adc->parent->setIntegerParam(adc->P_State, GADC_STATE_DONE);
    }
}

static asynStatus gadc_set_trigger(gadc_t * adc, epicsInt32 trigger)
{
    gadc_state_transition(adc, GADC_EVENT_TRIGGER);
    return asynSuccess;
}

static asynStatus gadc_set_enabled(gadc_t * adc, epicsInt32 enabled)
{
    if(enabled)
    {
        gadc_state_transition(adc, GADC_EVENT_RESET);
    }
    return asynSuccess;
}
*/

asynStatus WaveformPort::setClear(epicsInt32 clear)
{
    /* value is ignored */
    if(bsize != 0)
    {
        memset(buffer, 0, bsize * sizeof(epicsInt32));
    }
    return asynSuccess;
}

asynStatus WaveformPort::setInfo(epicsInt32 dummy)
{
    printf("gadc delegate: %s\n", portName);
    printf("state %d support 0x%x mode %d samples %d\n",
           IP(P_State), IP(P_Support), IP(P_Mode), IP(P_Samples));
    printf("buffer size %d\n", bsize);
    return asynSuccess;
}

// for SCAN emulation
asynStatus WaveformPort::setInterrupt(epicsInt32 dummy)
{
    size_t nIn;
    readInt32Array(NULL, outbuffer, bsize, &nIn);
    doCallbacksInt32Array(outbuffer, bsize, P_Waveform, 0);
    return asynSuccess;
}

asynStatus WaveformPort::setPutsample(epicsInt32 sample)
{
    if(bsize == 0)
    {
        return asynError;
    }
    /*
    if(!IP(P_Capture))
    {
        return asynSuccess;
    }
    */
    push_back(sample);
    samplecount++; // TODO: BUFFERCOUNT?
    if(samplecount == IP(P_Samples))
    {
        samplecount = 0;
        setInterrupt(1);
    }
    return asynSuccess;
}

/*
asynStatus gadc_put_sample(gadc_t * adc, GADC_DATA_t sample)
{
    if(adc->bsize == 0)
    {
        return asynError;
    }
    if(!INT(P_Capture))
    {
        return asynSuccess;
    }
    if(INT(P_Mode) == GADC_MODE_CONTINUOUS)
    {
        gadc_push_back(adc, sample);
        adc->samplecount++;
        if(adc->samplecount == INT(P_Samples))
        {
            adc->samplecount = 0;
            gadc_set_interrupt(adc, 1);
        }
    }
    else if(INT(P_Mode) == GADC_MODE_TRIGGERED)
    {
        if(INT(P_State) == GADC_STATE_WAITING)
        {
            gadc_push_back(adc, sample);
        }
        else if(INT(P_State) == GADC_STATE_TRIGGERED)
        {
            gadc_push_back(adc, sample);
            adc->samplecount++;
            if(adc->samplecount >= INT(P_Samples) + INT(P_Offset))
            {
                gadc_state_transition(adc, GADC_EVENT_DONE);
                gadc_set_interrupt(adc, 1);
            }
        }
        else if(INT(P_State) == GADC_STATE_DONE)
        {
            // discard sample, wait for enable
        }
    }
    else
    {
        return asynError;
    }
    adc->parent->callParamCallbacks();
    return asynSuccess;
}
*/

 /*
static asynStatus gadc_get_value(gadc_t * adc, GADC_DATA_t * value)
{
    if(adc->bsize == 0)
    {
        return asynError;
    }
    double sum = 0;
    int size = _max(_min(INT(P_Samples), INT(P_Average)), 1);
    int ofs = adc->bofs;
    int n;
    for(n = 0; n < size; n++)
    {
        ofs--;
        if(ofs < 0)
        {
            ofs += adc->bsize;
        }
        sum += adc->buffer[ofs];
    }
    sum /= size;
    *value = (GADC_DATA_t)sum;
    return asynSuccess;
}

 */

asynStatus WaveformPort::readInt32Array(
    asynUser * pasynUser, epicsInt32 * value, size_t nElements, size_t * nIn)
{
    if(pasynUser == NULL)
    {
        // this is ok, we are calling from setInterrupt
    }
    if(bsize == 0)
    {
        return asynError;
    }
    if(IP(P_Mode) == GADC_MODE_CONTINUOUS)
    {
        *nIn = _min(nElements, IP(P_Samples));
        int ofs = posmod(bofs - *nIn, bsize);
        int n;
        for(n = 0; n < (int)*nIn; n++)
        {
            value[n] = buffer[ofs];
            ofs++;
            if(ofs == bsize)
            {
                ofs = 0;
            }
        }
    }
    return asynSuccess;
}

/*
static asynStatus gadc_get_waveform_value(gadc_t * adc, GADC_DATA_t * value,
                                          size_t nElements, size_t * nIn)
{
    if(adc->bsize == 0)
    {
        return asynError;
    }
    if(INT(P_Mode) == GADC_MODE_CONTINUOUS)
    {
        *nIn = _min(nElements, INT(P_Samples));
        int ofs = posmod(adc->bofs - *nIn, adc->bsize);
        int n;
        for(n = 0; n < (int)*nIn; n++)
        {
            value[n] = adc->buffer[ofs];
            ofs++;
            if(ofs == adc->bsize)
            {
                ofs = 0;
            }
        }
    }
    else if(INT(P_Mode) == GADC_MODE_TRIGGERED || INT(P_Mode) == GADC_MODE_GATED)
    {
        if(INT(P_State) == GADC_STATE_DONE)
        {
            *nIn = _min(nElements, INT(P_Samples) - adc->readcount);
            int ofs = posmod(adc->bofs - INT(P_Samples) + adc->readcount, adc->bsize);
            int n;
            for(n = 0; n < (int)*nIn; n++)
            {
                value[n] = adc->buffer[ofs];
                ofs++;
                if(ofs == adc->bsize)
                {
                    ofs = 0;
                }
            }
            adc->readcount += *nIn;
            if(adc->readcount == INT(P_Samples) && INT(P_Retrigger))
            {
                gadc_set_enabled(adc, 1);
            }
        }
        else
        {
            *nIn = _min(nElements, INT(P_Samples));
            int ofs = posmod(adc->bofs - *nIn, adc->bsize);
            int n;
            for(n = 0; n < (int)*nIn; n++)
            {
                value[n] = adc->buffer[ofs];
                ofs++;
                if(ofs == adc->bsize)
                {
                    ofs = 0;
                }
            }
        }
    }
    return asynSuccess;
}
*/
