
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include <ellLib.h>

#include "classes.h"

void fill_in(int bytes, double value, void *start, int index)
{
    assert(bytes == 1 || bytes == 2 ||  bytes == 4);
    switch (bytes)
    {
        case 1:
            *( (uint8_t *) start + index) = (uint8_t) value;
            break;
        case 2:
            *( (uint16_t *) start + index) = (uint16_t) value;
            break;
        case 4:
            *( (uint32_t *) start + index) = (uint32_t) value;
            break;
    }
}

void copy_in(int bytes, void *start, int index, 
             uint8_t *pd, int offset, int bit_position)
{
    assert(bytes == 1 || bytes == 2 ||  bytes == 4);
    switch(bytes)
    {
        case 1:
            * (uint8_t *)(pd + offset) = *( (uint8_t *) start + index);
            break;
        case 2:
            * (uint16_t *)(pd + offset) = *( (uint16_t *) start + index);
            break;
        case 4:
            * (uint32_t *)(pd + offset) = *( (uint32_t *) start + index);
            break;
    }
}

void fill_in_constant(st_signal * signal, int bytes)
{
    signal->no_samples = 1;
    signal->perioddata = calloc(1, bytes);
    assert(signal->perioddata);
    fill_in(bytes, signal->signalspec->params.pconst.value, 
            signal->perioddata, 0);
}

void fill_in_squarewave(st_signal * signal, int bytes)
{
    signal->no_samples = (int) signal->signalspec->params.psquare.period_ms;
    signal->perioddata = calloc(signal->no_samples, bytes);
    assert(signal->perioddata);
    int i;
    for (i=0; i < signal->no_samples ; i++)
    {
        if (i < signal->no_samples / 2)
            fill_in(bytes, signal->signalspec->params.psquare.low,
                    signal->perioddata, i);
        else
            fill_in(bytes, signal->signalspec->params.psquare.high,
                    signal->perioddata, i);
    }
}

void fill_in_sinewave(st_signal * signal, int bytes)
{
    signal->no_samples = (int) signal->signalspec->params.psine.period_ms;
    signal->perioddata = calloc(signal->no_samples, bytes);
    assert(signal->perioddata);
    int i;
    double angle, value;
    for (i=0; i < signal->no_samples ; i++)
    {
        angle = (M_PI * 2.0 * i) / signal->no_samples;
        value = signal->signalspec->params.psine.low +
            (signal->signalspec->params.psine.high - 
             signal->signalspec->params.psine.low ) * sin(angle);
        fill_in(bytes, value, signal->perioddata, i);
    }
}

void fill_in_ramp(st_signal * signal, int bytes)
{
    signal->no_samples = (int) signal->signalspec->params.pramp.period_ms;
    signal->perioddata = calloc(signal->no_samples, bytes);
    assert(signal->perioddata);
    int i;
    int no_x; // integer count where the samples are "high"
    int no_samples = signal->no_samples;
    double value;
    double low = signal->signalspec->params.pramp.low;
    double high = signal->signalspec->params.pramp.high;

    no_x = (int) ( signal->signalspec->params.pramp.symmetry / 100.0 * no_samples);
    for (i=0; i < signal->no_samples ; i++)
    {
        if (i < no_x)
           value = low + ((high - low) * i )/ no_x;
        else
            value = high - (high - low) * ( i - no_x) / (no_samples - no_x);
        fill_in(bytes, value, signal->perioddata, i);
    }
}

void simulation_fill(st_signal * signal)
{
    assert(signal && signal->signalspec);
    assert(!signal->perioddata);
    assert(signal->signalspec->type != ST_INVALID);

    int bit_length = signal->signalspec->bit_length;
    int bytes = (int) ( ( bit_length - 1) / 8 ) + 1;
    assert( bytes <= 4);
    if (bytes == 3)
        bytes = 4;
    switch (signal->signalspec->type)
    {
        case ST_SQUAREWAVE:
            fill_in_squarewave(signal, bytes);
            break;
        case ST_SINEWAVE:
            fill_in_sinewave(signal, bytes);
            break;
        case ST_RAMP:
            fill_in_ramp(signal, bytes);
            break;
        default:
            fill_in_constant(signal, bytes);
    }
    // assume low and high are given in integer values
}

void copy_sim_data2(st_signal * signal, EC_PDO_ENTRY_MAPPING * pdo_entry_mapping,
                    uint8_t * pd, int index)
{
    assert(signal && signal->signalspec);
    assert(signal->perioddata);
    assert(signal->signalspec->type != ST_INVALID);
    
    int bit_length = signal->signalspec->bit_length;
    int bytes = (int) ( ( bit_length - 1) / 8 ) + 1;
    assert( bytes <= 4);
    if (bytes == 3)
        bytes = 4;
    copy_in(bytes, signal->perioddata, signal->index, pd, 
        pdo_entry_mapping->offset + bytes * index, pdo_entry_mapping->bit_position );
}

void copy_sim_data(st_signal * signal, EC_PDO_ENTRY_MAPPING * pdo_entry_mapping,
                    uint8_t * pd)
{
    copy_sim_data2(signal, pdo_entry_mapping, pd, 0);
}

