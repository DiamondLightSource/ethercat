/*

Generic ADC driver
TDI-CTRL-REQ-015

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "gadc.h"
#include "messages.h"
#include "rtutils.h"
#include "ecAsyn.h"

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

// check in gadc.h definition
#define FIRST_WAVEFORM_COMMAND P_Capture
#define LAST_WAVEFORM_COMMAND P_Waveform

#define NUM_WAVEFORM_PARAMS (&LAST_WAVEFORM_COMMAND - &FIRST_WAVEFORM_COMMAND + 1)

WaveformPort::WaveformPort(const char * name, ecAsyn *p, struct EC_PDO_ENTRY_MAPPING *m) : asynPortDriver(
    name,
    1, /* maxAddr */
    NUM_WAVEFORM_PARAMS, /* max parameters */
    asynInt32Mask | asynInt32ArrayMask | asynDrvUserMask, /* interface mask*/
    asynInt32Mask | asynInt32ArrayMask, /* interrupt mask */
    0, /* non-blocking, no addresses */
    1, /* autoconnect */
    0, /* default priority */
    0), /* default stack size */
 parentPort(p), mapping(m)
{
    printf("Creating waveform port \"%s\"\n", name);
    createParam("CAPTURE", asynParamInt32, &P_Capture);
    createParam("MODE", asynParamInt32, &P_Mode);
    createParam("SAMPLES", asynParamInt32, &P_Samples);
    createParam("OFFSET", asynParamInt32, &P_Offset);
    createParam("AVERAGE", asynParamInt32, &P_Average);
    createParam("CHANBUFF", asynParamInt32, &P_Chanbuff);
    createParam("TRIGGER", asynParamInt32, &P_Trigger);
    createParam("ENABLED", asynParamInt32, &P_Enabled);
    createParam("RETRIGGER", asynParamInt32, &P_Retrigger);
    createParam("CLEAR", asynParamInt32, &P_Clear);
    createParam("OVERFLOW", asynParamInt32, &P_Overflow);
    createParam("AVERAGEOVERFLOW", asynParamInt32, &P_Averageoverflow);
    createParam("BUFFERCOUNT", asynParamInt32, &P_Buffercount);
    createParam("STATE", asynParamInt32, &P_State);
    createParam("SUPPORT", asynParamInt32, &P_Support);
    createParam("INFO", asynParamInt32, &P_Info);
    createParam("PUTSAMPLE", asynParamInt32, &P_Putsample);
    createParam("VALUE", asynParamInt32, &P_Value);
    createParam("INTERRUPT", asynParamInt32, &P_Interrupt);
    createParam("WAVEFORM", asynParamInt32Array, &P_Waveform);
    setIntegerParam(P_State, GADC_STATE_WAITING);
    setIntegerParam(P_Support, GADC_BIT_TRIGGER | GADC_BIT_NEGATIVE_OFFSET);
}

#undef FIRST_WAVEFORM_COMMMAND
#undef LAST_WAVEFORM_COMMAND
#undef NUM_WAVEFORM_PARAMS


// note possible enhancement for asynPortDriver, setter function pointer table
asynStatus WaveformPort::writeInt32(asynUser * pasynUser, epicsInt32 value)
{
    asynStatus result = asynPortDriver::writeInt32(pasynUser, value);
    if(result != asynSuccess)
    {
        return result;
    }
    int cmd = pasynUser->reason;

    if(cmd == P_Mode)
    {
        return setMode(value);
    }
    else if(cmd == P_Samples)
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

asynStatus WaveformPort::getBounds(asynUser * pasynUser, epicsInt32 * low, epicsInt32 * high)
{
    assert(this->parentPort != NULL);
    return  parentPort->getBoundsForMapping(mapping, low, high);
}

asynStatus WaveformPort::setMode(epicsInt32 value)
{
    reset();
    callParamCallbacks();
    return asynSuccess;
}

void WaveformPort::reset()
{
    setIntegerParam(P_Buffercount, 0);
    setIntegerParam(P_State, GADC_STATE_WAITING);
    callParamCallbacks();
}

asynStatus WaveformPort::resize()
{
    free(buffer);
    free(outbuffer);
    bsize = 0;
    bofs = 0;
    reset();
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
        setIntegerParam(P_Enabled, 1);
        reset();
    }
    else
    {
        setIntegerParam(P_Enabled, 0);
    }
    callParamCallbacks();
    return asynSuccess;
}

asynStatus WaveformPort::setClear(epicsInt32 clear)
{
    if(buffer != NULL)
    {
        memset(buffer, 0, bsize * sizeof(epicsInt32));
    }
    reset();
    callParamCallbacks();
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
    getArrayValue(pasynUserSelf, outbuffer, bsize, &nIn);
    doCallbacksInt32Array(outbuffer, nIn, P_Waveform, 0);
    callParamCallbacks();
    return asynSuccess;
}

asynStatus WaveformPort::setTrigger(epicsInt32 dummy)
{
    if(IP(P_Enabled))
    {
        setIntegerParam(P_Buffercount, -IP(P_Offset));
        setIntegerParam(P_State, GADC_STATE_TRIGGERED);
        setEnabled(0);
    }
    callParamCallbacks();
    return asynSuccess;
}

int WaveformPort::calcStartOffset(int size)
{
    if(IP(P_Mode) == GADC_MODE_TRIGGERED)
    {
        return posmod(bofs - _max(-IP(P_Offset), size), bsize);
    }
    else
    {
        return posmod(bofs - size, bsize);
    }
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
    push_back(sample);
    if((IP(P_Mode) == GADC_MODE_CONTINUOUS) || 
       (IP(P_Mode) == GADC_MODE_TRIGGERED && IP(P_State) == GADC_STATE_TRIGGERED))
    {
        setIntegerParam(P_Buffercount, IP(P_Buffercount) + 1);
    }
    if(IP(P_Buffercount) >= IP(P_Samples))
    {
        setInterrupt(1);
        setIntegerParam(P_Buffercount, 0);
    }
    callParamCallbacks();
    return asynSuccess;
}

asynStatus WaveformPort::getArrayValue(
    asynUser * pasynUser, epicsInt32 * value, size_t nElements, size_t * nIn)
{
    if(bsize == 0)
    {
        return asynError;
    }
    int size = _min(nElements, IP(P_Samples));
    *nIn = size;
    int ofs = calcStartOffset(size);
    int n;
    for(n = 0; n < size; n++)
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
    int ofs = calcStartOffset(size);
    int n;
    for(n = 0; n < size; n++)
    {
        sum += buffer[ofs];
        ofs++;
        if(ofs == bsize)
        {
            ofs = 0;
        }
    }
    sum /= size;
    *value = (epicsInt32)sum;
    return asynSuccess;
}

/*
offset
zero          ................TDDDDDX...........
positive      ................T...........DDDDDX
negative      DDDDDDDDDDDDDDDDTDDDDDX...........
very negative DDDDDDD.........TX................
*/
