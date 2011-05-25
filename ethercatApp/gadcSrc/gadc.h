/*

Generic ADC driver
TDI-CTRL-REQ-015

*/

enum
{
    GADC_EVENT_TRIGGER = 0,
    GADC_EVENT_DONE = 1,
    GADC_EVENT_RESET = 2
};

enum
{
    GADC_MODE_CONTINUOUS = 0,
    GADC_MODE_TRIGGERED = 1,
    GADC_MODE_GATED = 2
};

enum
{
    GADC_STATE_WAITING = 0,
    GADC_STATE_TRIGGERED = 1,
    GADC_STATE_DONE = 2
};

enum
{
    GADC_BIT_GATE = 0x01,
    GADC_BIT_TRIGGER = 0x02,
    GADC_BIT_NEGATIVE_OFFSET = 0x04
};

typedef epicsInt32 GADC_DATA_t;

struct gadc_t;
typedef struct gadc_t gadc_t;

struct gadc_t
{
    ELLNODE node;

    /* parameters */
    epicsInt32 _capture;
    epicsInt32 _mode;
    epicsInt32 _samples;
    epicsInt32 _offset;
    epicsInt32 _average;
    epicsInt32 _chanBuff;
    epicsInt32 _enabled;
    epicsInt32 _retrigger;
    epicsInt32 _overflow;
    epicsInt32 _averageOverflow;
    epicsInt32 _bufferCount;
    epicsInt32 _state;
    epicsInt32 _support;
    
    /* private */
    char * name;
    asynPortDriver * parent;
    GADC_DATA_t * buffer;
    int bofs;
    int bsize;
    int samplecount;
    int readcount;
    int P_First;
    int P_Last;
    int channel;
};

gadc_t * gadc_new(asynPortDriver * parent, int channel);
asynStatus gadc_readInt32(gadc_t * adc, int reason, epicsInt32 * value);
asynStatus gadc_writeInt32(gadc_t * adc, int reason, epicsInt32 value);
asynStatus gadc_readInt32Array(gadc_t * adc, int reason, epicsInt32 * value, size_t nElements, size_t * nIn);
int gadc_has_parameter(gadc_t * adc, int reason);
int gadc_find_parameter(gadc_t * adc, char * name);
const char * gadc_parameter_name(gadc_t * adc, int reason);
asynStatus gadc_writeInt32_name(gadc_t * adc, char * name, epicsInt32 value);
int gadc_get_num_parameters();
asynStatus gadc_put_sample(gadc_t * adc, GADC_DATA_t sample);
