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

static asynStatus gadc_get_waveform_value(gadc_t * adc, GADC_DATA_t * value,
                                          size_t nElements, size_t * nIn);
static asynStatus gadc_set_enabled(gadc_t * adc, epicsInt32 enabled);

static int _max(int a, int b)
{
    return a > b ? a : b;
}

static int _min(int a, int b)
{
    return a < b ? a : b;
}

epicsInt32 getIntegerParam(gadc_t * adc, int param)
{
    epicsInt32 value;
    if(adc->parent->getIntegerParam(param, &value) != asynSuccess)
    {
        value = 0;
    }
    return value;
}

#define INT(name) getIntegerParam(adc, adc->name)

static asynStatus gadc_resize(gadc_t * adc)
{
    free(adc->buffer);
    adc->bsize = 0;
    adc->bofs = 0;
    int size = _max(INT(P_Samples), -INT(P_Offset));
    printf("resize %d\n", size);
    if(size > INT(P_Chanbuff))
    {
        printf("can't resize %d %d\n", size, INT(P_Chanbuff));
        return asynError;
    }
    adc->buffer = (GADC_DATA_t *)calloc(size, sizeof(GADC_DATA_t));
    if(adc->buffer == NULL)
    {
        return asynError;
    }
    adc->bsize = size;
    return asynSuccess;
}

/* parameters */

static asynStatus gadc_set_samples(gadc_t * adc, epicsInt32 samples)
{
    if(samples < 0)
    {
        return asynError;
    }
    else
    {
        return gadc_resize(adc);
    }
}

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


static asynStatus gadc_set_chanBuff(gadc_t * adc, epicsInt32 chanBuff)
{
    return gadc_resize(adc);
}

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
    /* value is ignored */
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

static asynStatus gadc_set_clear(gadc_t * adc, epicsInt32 clear)
{
    /* value is ignored */
    if(adc->bsize != 0)
    {
        memset(adc->buffer, 0, adc->bsize * sizeof(GADC_DATA_t));
    }
    return asynSuccess;
}

static asynStatus gadc_set_info(gadc_t * adc, epicsInt32 dummy)
{
    printf("gadc delegate: %s\n", adc->name);
    printf("state %d support 0x%x mode %d samples %d\n",
           INT(P_State), INT(P_Support), INT(P_Mode), INT(P_Samples));
    printf("buffer size %d\n", adc->bsize);
    return asynSuccess;
}

static void gadc_push_back(gadc_t * adc, GADC_DATA_t sample)
{
    adc->buffer[adc->bofs] = sample;
    adc->bofs = (adc->bofs + 1) % adc->bsize;
}

// for SCAN emulation
asynStatus gadc_set_interrupt(gadc_t * adc, epicsInt32 dummy)
{
    epicsInt32 * value = (epicsInt32 *)calloc(adc->bsize, sizeof(epicsInt32));
    size_t nIn;
    gadc_get_waveform_value(adc, value, adc->bsize, &nIn);
    adc->parent->doCallbacksInt32Array(value, adc->bsize, adc->P_Last, 0);
    free(value);
    return asynSuccess;
}

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

static int posmod(int a, int b)
{
    int c = a % b;
    while(c < 0)
    {
        c += b;
    }
    return c;
}

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
            /* return the intermediate result */
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

static const char * gadc_command_names [] = 
{
    "CAPTURE", 
    "MODE", 
    "SAMPLES",
    "OFFSET",
    "AVERAGE",
    "CHANBUFF",
    "TRIGGER",
    "ENABLED",
    "RETRIGGER",
    "CLEAR",
    "OVERFLOW",
    "AVERAGEOVERFLOW",
    "BUFFERCOUNT",
    "STATE",
    "SUPPORT",
    "INFO",
    "PUTSAMPLE",
    "VALUE",
    "INTERRUPT",
    "WAVEFORM"
};

static char * format(const char *fmt, ...)
{
    char * buffer = NULL;
    va_list args;
    va_start(args, fmt);
    int ret = vasprintf(&buffer, fmt, args);
    assert(ret != -1);
    va_end(args);
    return buffer;
}

#define createParamN(name, param, type, setter) \
    { \
        char * pn = format("ADC%d_%s", adc->channel, name); \
        assert(adc->parent->createParam(pn, type, &adc->param) == asynSuccess); \
        adc->setters[adc->param] = setter; \
        printf("made new param %s\n", pn); \
    }

static void gadc_make_parameters(gadc_t * adc)
{
    // FIXME make this generic to the parameter system
    // need asynPortDriver subclass with overrides (classes or pointers?)
    adc->setters = (setter_epicsInt32 *)calloc(1000, sizeof(setter_epicsInt32));
    // create parameters with callbacks
    createParamN("CAPTURE", P_Capture, asynParamInt32, NULL);
    adc->P_First = adc->P_Capture;
    createParamN("MODE", P_Mode, asynParamInt32, NULL);
    createParamN("SAMPLES", P_Samples, asynParamInt32, gadc_set_samples);
    createParamN("OFFSET", P_Offset, asynParamInt32, gadc_set_offset);
    createParamN("AVERAGE", P_Average, asynParamInt32, NULL);
    createParamN("CHANBUFF", P_Chanbuff, asynParamInt32, gadc_set_chanBuff);
    createParamN("TRIGGER", P_Trigger, asynParamInt32, gadc_set_trigger);
    createParamN("ENABLED", P_Enabled, asynParamInt32, gadc_set_enabled);
    createParamN("RETRIGGER", P_Retrigger, asynParamInt32, NULL);
    createParamN("CLEAR", P_Clear, asynParamInt32, gadc_set_clear);
    createParamN("OVERFLOW", P_Overflow, asynParamInt32, NULL);
    createParamN("AVERAGEOVERFLOW", P_Averageoverflow, asynParamInt32, NULL);
    createParamN("BUFFERCOUNT", P_Buffercount, asynParamInt32, NULL);
    createParamN("STATE", P_State, asynParamInt32, NULL);
    createParamN("SUPPORT", P_Support, asynParamInt32, NULL);
    createParamN("INFO", P_Info, asynParamInt32, gadc_set_info);
    createParamN("PUTSAMPLE", P_Putsample, asynParamInt32, gadc_put_sample);
    createParamN("VALUE", P_Value, asynParamInt32, NULL);
    createParamN("INTERRUPT", P_Interrupt, asynParamInt32, gadc_set_interrupt);
    createParamN("WAVEFORM", P_Waveform, asynParamInt32Array, NULL);
    adc->P_Last = adc->P_Waveform;

    adc->parent->setIntegerParam(adc->P_Support, GADC_BIT_TRIGGER | GADC_BIT_NEGATIVE_OFFSET);
    adc->parent->setIntegerParam(adc->P_State, GADC_STATE_WAITING);
    
}

/* public */

gadc_t * gadc_new(asynPortDriver * parent, int channel)
{
    gadc_t * adc = (gadc_t *)calloc(1, sizeof(gadc_t));
    adc->name = format("ADC%d", channel);
    adc->channel = channel;
    adc->parent = parent;
    gadc_make_parameters(adc);
    return adc;
}

int gadc_get_num_parameters()
{
    return sizeof(gadc_command_names) / sizeof(gadc_command_names[0]);
}

setter_epicsInt32 gadc_get_write_function(gadc_t * adc, int reason)
{
    return adc->setters[reason];
}

const char * gadc_parameter_name(gadc_t * adc, int reason)
{
    int cmd = reason - adc->P_First;
    return gadc_command_names[cmd];
}

asynStatus gadc_writeInt32(gadc_t * adc, int reason, epicsInt32 value)
{
    if(adc->setters[reason] != NULL)
    {
        if(reason != adc->P_Interrupt)
        {
            printf("ADC write param %s -> %d\n", gadc_parameter_name(adc, reason), value);
        }
        asynStatus status = adc->setters[reason](adc, value);
        adc->parent->callParamCallbacks();
        return status;
    }
    return asynError;
}
