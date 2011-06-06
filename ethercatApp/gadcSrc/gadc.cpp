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
    // printf("write param %d %d\n", cmd, value);
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


asynStatus WaveformPort::setOffset(epicsInt32 offset)
{
    if(IP(P_Offset) != offset)
    {
        return resize();
    }
    else
    {
        return asynSuccess;
    }
}

asynStatus WaveformPort::setChanbuff(epicsInt32 chanBuff)
{
    return resize();
}

asynStatus WaveformPort::setEnabled(epicsInt32 enabled)
{
    if(enabled)
    {
        setIntegerParam(P_Buffercount, 0);
        setIntegerParam(P_Enabled, 1);
        setIntegerParam(P_State, GADC_STATE_WAITING);
    }
    callParamCallbacks();
    return asynSuccess;
}

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
    epicsInt32 value;
    if(getValue(pasynUserSelf, &value) == asynSuccess)
    {
        setIntegerParam(P_Value, value);
    }
    readInt32Array(pasynUserSelf, outbuffer, bsize, &nIn);
    doCallbacksInt32Array(outbuffer, bsize, P_Waveform, 0);
    callParamCallbacks();
    return asynSuccess;
}

asynStatus WaveformPort::setTrigger(epicsInt32 dummy)
{
    printf("triggered\n");
    if(IP(P_Enabled))
    {
        setIntegerParam(P_Buffercount, IP(P_Offset));
        setIntegerParam(P_State, GADC_STATE_TRIGGERED);
    }
    callParamCallbacks();
    return asynSuccess;
}

asynStatus WaveformPort::setPutsample(epicsInt32 sample)
{
    if(bsize == 0)
    {
        return asynError;
    }
    if(!IP(P_Capture))
    {
        return asynSuccess;
    }
    if(IP(P_Mode) == GADC_MODE_CONTINUOUS)
    {
        push_back(sample);
        setIntegerParam(P_Buffercount, IP(P_Buffercount) + 1);
        if(IP(P_Buffercount) == IP(P_Samples))
        {
            setIntegerParam(P_Buffercount, 0);
            setInterrupt(1);
        }
    }
    else if(IP(P_State) == GADC_STATE_TRIGGERED)
    {
        if(IP(P_Buffercount) < IP(P_Samples))
        {
            push_back(sample);
            setIntegerParam(P_Buffercount, IP(P_Buffercount) + 1);
        }
        else
        {
            setInterrupt(1);
        }
    }
    callParamCallbacks();
    return asynSuccess;
}

asynStatus WaveformPort::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    if(pasynUser->reason == P_Value)
    {
        return getValue(pasynUser, value);
    }
    else
    {
        return asynPortDriver::readInt32(pasynUser, value);
    }
}

asynStatus WaveformPort::readInt32Array(
    asynUser * pasynUser, epicsInt32 * value, size_t nElements, size_t * nIn)
{
    if(bsize == 0)
    {
        return asynError;
    }
    // read most recent data
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
    if(IP(P_Mode) == GADC_MODE_TRIGGERED)
    {
        if(IP(P_Retrigger))
        {
            setEnabled(1);
        }
        else
        {
            setIntegerParam(P_State, GADC_STATE_DONE);
        }
    }
    callParamCallbacks();
    return asynSuccess;
}

asynStatus WaveformPort::getValue(asynUser * pasynUser, epicsInt32 * value)
{
    if(bsize == 0)
    {
        return asynError;
    }
    double sum = 0;
    int size = _max(_min(IP(P_Samples), IP(P_Average)), 1);
    int ofs = bofs;
    int n;
    for(n = 0; n < size; n++)
    {
        ofs--;
        if(ofs < 0)
        {
            ofs += bsize;
        }
        sum += buffer[ofs];
    }
    sum /= size;
    *value = (epicsInt32)sum;
    return asynSuccess;
}
