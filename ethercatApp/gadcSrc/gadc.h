/*

Generic ADC driver
TDI-CTRL-REQ-015

*/
#include <asynPortDriver.h>

class WaveformPort : public asynPortDriver
{

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

    int P_Capture;
#define FIRST_WAVEFORM_COMMAND P_Capture
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
#define LAST_WAVEFORM_COMMAND P_Waveform
    epicsInt32 * buffer;
    epicsInt32 * outbuffer;
    int bofs;
    int bsize;
    int channel;
    asynStatus resize();
    void reset();
    epicsInt32 IP(int param)
    {
        epicsInt32 value;
        if(getIntegerParam(param, &value) != asynSuccess)
        {
            value = 0;
        }
        return value;
    }
    void push_back(epicsInt32 sample)
    {
        assert(bofs >= 0 && bofs < bsize);
        buffer[bofs] = sample;
        bofs = (bofs + 1) % bsize;
        assert(bofs >= 0 && bofs < bsize);
    }
    int calcStartOffset(int size);
public:
    WaveformPort(const char * name);
    virtual asynStatus writeInt32(asynUser * pasynUser, epicsInt32 value);
    /* does NOT support SCAN */
    asynStatus getArrayValue(asynUser *pasynUser, epicsInt32 *value,
                             size_t nElements, size_t *nIn);
    asynStatus getValue(asynUser * pasynUser, epicsInt32 * value);
    asynStatus setMode(epicsInt32 value);
    asynStatus setSamples(epicsInt32 value);
    asynStatus setOffset(epicsInt32 value);
    asynStatus setChanbuff(epicsInt32 value);
    asynStatus setTrigger(epicsInt32 value);
    asynStatus setEnabled(epicsInt32 value);
    asynStatus setClear(epicsInt32 value);
    asynStatus setInfo(epicsInt32 value);
    asynStatus setInterrupt(epicsInt32 value);
    asynStatus setPutsample(epicsInt32 value);
};

#define NUM_WAVEFORM_PARAMS (&LAST_WAVEFORM_COMMAND - &FIRST_WAVEFORM_COMMAND + 1)

WaveformPort::WaveformPort(const char * name) : asynPortDriver(
    name,
    1, /* maxAddr */
    NUM_WAVEFORM_PARAMS, /* max parameters */
    asynInt32Mask | asynInt32ArrayMask | asynDrvUserMask, /* interface mask*/
    asynInt32Mask | asynInt32ArrayMask, /* interrupt mask */
    0, /* non-blocking, no addresses */
    1, /* autoconnect */
    0, /* default priority */
    0) /* default stack size */
{
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

