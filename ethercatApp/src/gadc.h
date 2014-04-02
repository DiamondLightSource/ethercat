#ifndef _gadc_H_
#define _gadc_H_
/*

Generic ADC driver
TDI-CTRL-REQ-015

*/
#include <asynPortDriver.h>

class ecAsyn;
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

    /* on change of parameters, modify 
       FIRST_WAVEFORM_COMMAND and LAST_WAVEFORM_COMMAND 
       in gadc.cpp */
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

    epicsInt32 * buffer;
    epicsInt32 * outbuffer;
    int bofs;
    int bsize;
    int channel;
    ecAsyn *parentPort;
    struct EC_PDO_ENTRY_MAPPING *mapping;
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
    WaveformPort(const char * name, ecAsyn *p, struct EC_PDO_ENTRY_MAPPING *m);
    virtual asynStatus writeInt32(asynUser * pasynUser, epicsInt32 value);
    virtual asynStatus getBounds(asynUser * pasynUser, 
                                 epicsInt32 * low, epicsInt32 * high);
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


#endif // _gadc_H_
