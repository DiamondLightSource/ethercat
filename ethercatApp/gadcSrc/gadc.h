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

typedef asynStatus (*setter_epicsInt32)(gadc_t *, epicsInt32);

struct gadc_t
{

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
    
    /* Parameters */
    int P_Capture;
    int P_Mode;
    int P_Samples;
    int P_Offset;
    int P_Average;
    int P_Chanbuff;
    int P_Trigger;
    int P_Enabled;
    int P_Retrigger;
    int P_Clear;
    int P_Overflow;
    int P_Averageoverflow;
    int P_Buffercount;
    int P_State;
    int P_Support;
    int P_Info;
    int P_Putsample;
    int P_Value;
    int P_Interrupt;
    int P_Waveform;
    setter_epicsInt32 * setters;
};

gadc_t * gadc_new(asynPortDriver * parent, int channel);
asynStatus gadc_writeInt32(gadc_t * adc, int reason, epicsInt32 value);
const char * gadc_parameter_name(gadc_t * adc, int reason);

int gadc_get_num_parameters();
asynStatus gadc_put_sample(gadc_t * adc, GADC_DATA_t sample);
setter_epicsInt32 gadc_get_write_function(gadc_t * adc, int reason);
