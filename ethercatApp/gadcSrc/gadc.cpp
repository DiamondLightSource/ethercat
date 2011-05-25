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

static asynStatus gadc_set_enabled(gadc_t * adc, epicsInt32 enabled);

static int _max(int a, int b)
{
    return a > b ? a : b;
}

static int _min(int a, int b)
{
    return a < b ? a : b;
}

static asynStatus gadc_resize(gadc_t * adc)
{
    free(adc->buffer);
    adc->bsize = 0;
    adc->bofs = 0;
    int size = _max(adc->_samples, -adc->_offset);
    if(size > adc->_chanBuff)
    {
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

static asynStatus gadc_set_capture(gadc_t * adc, epicsInt32 capture)
{
    adc->_capture = capture;
    return asynSuccess;
}

static asynStatus gadc_get_capture(gadc_t * adc, epicsInt32 * capture)
{
    *capture = adc->_capture;
    return asynSuccess;
}

static asynStatus gadc_set_mode(gadc_t * adc, epicsInt32 mode)
{
    adc->_mode = mode;
    return asynSuccess;
}

static asynStatus gadc_get_mode(gadc_t * adc, epicsInt32 * mode)
{
    *mode = adc->_mode;
    return asynSuccess;
}

static asynStatus gadc_set_samples(gadc_t * adc, epicsInt32 samples)
{
    if(samples < 0)
    {
        return asynError;
    }
    else if(adc->_samples != samples)
    {
        adc->_samples = samples;
        return gadc_resize(adc);
    }
    else
    {
        return asynSuccess;
    }
}

static asynStatus gadc_get_samples(gadc_t * adc, epicsInt32 * samples)
{
    *samples = adc->_samples;
    return asynSuccess;
}

static asynStatus gadc_set_offset(gadc_t * adc, epicsInt32 offset)
{
    if(adc->_offset != offset)
    {
        return gadc_resize(adc);
    }
    else
    {
        return asynSuccess;
    }
}

static asynStatus gadc_get_offset(gadc_t * adc, epicsInt32 * offset)
{
    *offset = adc->_offset;
    return asynSuccess;
}

static asynStatus gadc_set_average(gadc_t * adc, epicsInt32 average)
{
    adc->_average = average;
    return asynSuccess;
}

static asynStatus gadc_get_average(gadc_t * adc, epicsInt32 * average)
{
    *average = adc->_average;
    return asynSuccess;
}

static asynStatus gadc_set_chanBuff(gadc_t * adc, epicsInt32 chanBuff)
{
    adc->_chanBuff = chanBuff;
    return gadc_resize(adc);
}

static asynStatus gadc_get_chanBuff(gadc_t * adc, epicsInt32 * chanBuff)
{
    *chanBuff = adc->_chanBuff;
    return asynSuccess;
}

static void gadc_state_transition(gadc_t * adc, int event)
{
    if(event == GADC_EVENT_RESET)
    {
        adc->_state = GADC_STATE_WAITING;
        adc->readcount = 0;
        adc->samplecount = 0;
    }
    else if(adc->_state == GADC_STATE_WAITING &&
            adc->_enabled && 
            (event == GADC_EVENT_TRIGGER))
    {
        adc->_state = GADC_STATE_TRIGGERED;
    }
    else if(adc->_state == GADC_STATE_TRIGGERED &&
            (event == GADC_EVENT_DONE))
    {
        gadc_set_enabled(adc, 0);
        adc->_state = GADC_STATE_DONE;
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
    adc->_enabled = enabled;
    if(enabled)
    {
        gadc_state_transition(adc, GADC_EVENT_RESET);
    }
    return asynSuccess;
}

static asynStatus gadc_get_enabled(gadc_t * adc, epicsInt32 * enabled)
{
    *enabled = adc->_enabled;
    return asynSuccess;
}

static asynStatus gadc_set_retrigger(gadc_t * adc, epicsInt32 retrigger)
{
    adc->_retrigger = retrigger;
    return asynSuccess;
}

static asynStatus gadc_get_retrigger(gadc_t * adc, epicsInt32 * retrigger)
{
    *retrigger = adc->_retrigger;
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

static asynStatus gadc_get_overflow(gadc_t * adc, epicsInt32 * overflow)
{
    *overflow = adc->_overflow;
    return asynSuccess;
}

static asynStatus gadc_get_averageOverflow(gadc_t * adc, epicsInt32 * averageOverflow)
{
    *averageOverflow = adc->_averageOverflow;
    return asynSuccess;
}

static asynStatus gadc_get_bufferCount(gadc_t * adc, epicsInt32 * bufferCount)
{
    *bufferCount = adc->_bufferCount;
    return asynSuccess;
}

static asynStatus gadc_get_state(gadc_t * adc, epicsInt32 * state)
{
    *state = adc->_state;
    return asynSuccess;
}

static asynStatus gadc_get_support(gadc_t * adc, epicsInt32 * support)
{
    *support = GADC_BIT_TRIGGER | GADC_BIT_NEGATIVE_OFFSET;
    return asynSuccess;
}

static asynStatus gadc_set_info(gadc_t * adc, epicsInt32 info)
{
    printf("gadc delegate: %s\n", adc->name);
    epicsInt32 state, support, mode, samples;
    gadc_get_mode(adc, &mode);
    gadc_get_samples(adc, &samples);
    gadc_get_state(adc, &state);
    gadc_get_support(adc, &support);
    printf("state %d support 0x%x mode %d samples %d\n",
           state, support, mode, samples);
    printf("buffer size %d\n", adc->bsize);
    return asynSuccess;
}

static void gadc_push_back(gadc_t * adc, GADC_DATA_t sample)
{
    adc->buffer[adc->bofs] = sample;
    adc->bofs = (adc->bofs + 1) % adc->bsize;
}

asynStatus gadc_put_sample(gadc_t * adc, GADC_DATA_t sample)
{
    if(adc->bsize == 0)
    {
        return asynError;
    }
    if(!adc->_capture)
    {
        return asynSuccess;
    }
    if(adc->_mode == GADC_MODE_CONTINUOUS)
    {
        gadc_push_back(adc, sample);
        adc->samplecount++;
        if(adc->samplecount == adc->_samples)
        {
            adc->samplecount = 0;
            // I/O INTERRUPT
            printf("I/O interrupt\n");
        }
    }
    else if(adc->_mode == GADC_MODE_TRIGGERED)
    {
        if(adc->_state == GADC_STATE_WAITING)
        {
            gadc_push_back(adc, sample);
        }
        else if(adc->_state == GADC_STATE_TRIGGERED)
        {
            gadc_push_back(adc, sample);
            adc->samplecount++;
            if(adc->samplecount >= adc->_samples + adc->_offset)
            {
                gadc_state_transition(adc, GADC_EVENT_DONE);
            }
        }
        else if(adc->_state == GADC_STATE_DONE)
        {
            // discard sample, wait for enable
        }
    }
    else
    {
        return asynError;
    }
    return asynSuccess;
}

static asynStatus gadc_get_value(gadc_t * adc, GADC_DATA_t * value)
{
    if(adc->bsize == 0)
    {
        return asynError;
    }
    double sum = 0;
    int size = _max(_min(adc->_samples, adc->_average), 1);
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
    if(adc->_mode == GADC_MODE_CONTINUOUS)
    {
        *nIn = _min(nElements, adc->_samples);
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
    else if(adc->_mode == GADC_MODE_TRIGGERED || adc->_mode == GADC_MODE_GATED)
    {
        if(adc->_state == GADC_STATE_DONE)
        {
            *nIn = _min(nElements, adc->_samples - adc->readcount);
            int ofs = posmod(adc->bofs - adc->_samples + adc->readcount, adc->bsize);
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
            if(adc->readcount == adc->_samples && adc->_retrigger)
            {
                gadc_set_enabled(adc, 1);
            }
        }
        else
        {
            /* return the intermediate result */
            *nIn = _min(nElements, adc->_samples);
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

typedef asynStatus (*getter_int32_array_t)(gadc_t *, int *, size_t nElements, size_t * nIn);
typedef asynStatus (*getter_epicsInt32)(gadc_t *, epicsInt32 *);
typedef asynStatus (*setter_epicsInt32)(gadc_t *, epicsInt32);

typedef struct
{
    const char * name;
    setter_epicsInt32 set;
    getter_epicsInt32 get;
    getter_int32_array_t getWave;
} command_t;

command_t gadc_commands[] =
{
    {"CAPTURE", gadc_set_capture, gadc_get_capture},
    {"MODE", gadc_set_mode, gadc_get_mode},
    {"SAMPLES", gadc_set_samples, gadc_get_samples},
    {"OFFSET", gadc_set_offset, gadc_get_offset},
    {"AVERAGE", gadc_set_average, gadc_get_average},
    {"CHANBUFF", gadc_set_chanBuff, gadc_get_chanBuff},
    {"TRIGGER", gadc_set_trigger, NULL},
    {"ENABLED", gadc_set_enabled, gadc_get_enabled},
    {"RETRIGGER", gadc_set_retrigger, gadc_get_retrigger},
    {"CLEAR", gadc_set_clear, NULL},
    {"OVERFLOW", NULL, gadc_get_overflow},
    {"AVERAGEOVERFLOW", NULL, gadc_get_averageOverflow},
    {"BUFFERCOUNT", NULL, gadc_get_bufferCount},
    {"STATE", NULL, gadc_get_state},
    {"SUPPORT", NULL, gadc_get_support},
    {"INFO", gadc_set_info, NULL},
    {"PUTSAMPLE", gadc_put_sample, NULL},
    {"VALUE", NULL, gadc_get_value},
    {"WAVEFORM", NULL, NULL, gadc_get_waveform_value}
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

static int gadc_connect_parameters(gadc_t * adc)
{
    adc->P_First = -1;
    adc->P_Last = -1;
    int size = sizeof(gadc_commands) / sizeof(gadc_commands[0]);
    for(int n = 0; n < size; n++)
    {
        int P_Next;
        char * name = format("ADC%d_%s", adc->channel, gadc_commands[n].name);
        adc->parent->createParam(name, asynParamInt32, &P_Next);
        if(adc->P_First == -1)
        {
            adc->P_First = P_Next;
        }
        assert(adc->P_Last == -1 || P_Next == adc->P_Last + 1);
        adc->P_Last = P_Next;
        printf("%s -> %d\n", name, P_Next);
    }
    return 0;
}

/* public */

gadc_t * gadc_new(asynPortDriver * parent, int channel)
{
    gadc_t * adc = (gadc_t *)calloc(1, sizeof(gadc_t));
    adc->name = format("ADC%d", channel);
    adc->channel = channel;
    adc->parent = parent;
    gadc_connect_parameters(adc);
    return adc;
}

int gadc_get_num_parameters()
{
    return sizeof(gadc_commands) / sizeof(gadc_commands[0]);
}

asynStatus gadc_writeInt32(gadc_t * adc, int reason, epicsInt32 value)
{
    assert(gadc_has_parameter(adc, reason));
    int cmd = reason - adc->P_First;
    if(gadc_commands[cmd].set != NULL)
    {
        printf("gadc write %s %d\n", gadc_commands[cmd].name, value);
        return gadc_commands[cmd].set(adc, value);
    }
    return asynError;
}

asynStatus gadc_readInt32(gadc_t * adc, int reason, epicsInt32 * value)
{
    assert(gadc_has_parameter(adc, reason));
    int cmd = reason - adc->P_First;
    if(gadc_commands[cmd].get != NULL)
    {
        return gadc_commands[cmd].get(adc, value);
    }
    return asynError;
}

asynStatus gadc_readInt32Array(gadc_t * adc, int reason, epicsInt32 * value, size_t nElements, size_t * nIn)
{
    assert(gadc_has_parameter(adc, reason));
    int cmd = reason - adc->P_First;
    if(gadc_commands[cmd].getWave != NULL)
    {
        return gadc_commands[cmd].getWave(adc, value, nElements, nIn);
    }
    return asynError;
}


int gadc_has_parameter(gadc_t * adc, int reason)
{
    return reason >= adc->P_First && reason <= adc->P_Last;
}

int gadc_find_parameter(gadc_t * adc, char * name)
{
    int size = sizeof(gadc_commands) / sizeof(gadc_commands[0]);
    for(int n = 0; n < size; n++)
    {
        if(strcmp(gadc_commands[n].name, name) == 0)
        {
            return n + adc->P_First;
        }
    }
    return -1;
}

const char * gadc_parameter_name(gadc_t * adc, int reason)
{
    assert(gadc_has_parameter(adc, reason));
    int cmd = reason - adc->P_First;
    return gadc_commands[cmd].name;
}

asynStatus gadc_writeInt32_name(gadc_t * adc, char * name, epicsInt32 value)
{
    int reason = gadc_find_parameter(adc, name);
    return gadc_writeInt32(adc, reason, value);
}
