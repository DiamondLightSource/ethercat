template <class T> struct ListNode
{
    ELLNODE node;
};

class WriteObserver : public ListNode<WriteObserver>
{
public:
    virtual asynStatus writeInt32(asynUser * pasynUser, epicsInt32 value) = 0;
    virtual ~WriteObserver() {}
};

class ProcessDataObserver : public ListNode<ProcessDataObserver>
{
public:
    virtual void on_pdo_message(PDO_MESSAGE * message, int size) = 0;
    virtual ~ProcessDataObserver() {}
};

class GADCWriteObserver : public WriteObserver
{
    gadc_t * adc;
public:
    GADCWriteObserver(gadc_t * adc) : adc(adc) {}
    virtual asynStatus writeInt32(asynUser * pasynUser, epicsInt32 value)
    {
        return gadc_writeInt32(adc, pasynUser->reason, value);
    }
};

class ProcessDataWriteObserver : public WriteObserver
{
    rtMessageQueueId writeq;
    EC_PDO_ENTRY_MAPPING * mapping;
public:
    ProcessDataWriteObserver(rtMessageQueueId writeq, EC_PDO_ENTRY_MAPPING * mapping) : 
        writeq(writeq), mapping(mapping) {}
    virtual asynStatus writeInt32(asynUser * pasynUser, epicsInt32 value)
    {
        WRITE_MESSAGE write;
        write.tag = MSG_WRITE;
        write.offset = mapping->offset;
        write.bit_position = mapping->bit_position;
        write.bits = mapping->pdo_entry->bits;
        write.value = value;
        rtMessageQueueSend(writeq, &write, sizeof(WRITE_MESSAGE));
        return asynSuccess;
    }
};

class ecAsyn : public asynPortDriver, public ProcessDataObserver
{
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    int pdos;
    int devid;
    rtMessageQueueId writeq;
    int P_AL_STATE;
    int P_ERROR_FLAG;
    int P_DISABLE;
    ELLLIST samplers;
    ELLLIST pdo_delegates;
public:
    WriteObserver ** write_delegates;
    ecAsyn(EC_DEVICE * device, int pdos, rtMessageQueueId writeq, int devid);
    EC_DEVICE * device;
    virtual void on_pdo_message(PDO_MESSAGE * message, int size);
};

class ecMaster : public asynPortDriver, public ProcessDataObserver
{
    int P_Cycle;
#define FIRST_MASTER_COMMAND P_Cycle
    int P_WorkingCounter;
    int P_Missed;
    int P_WcState;
#define LAST_MASTER_COMMAND P_WcState
    epicsInt32 lastCycle;
    epicsInt32 missed;
public:
    ecMaster(char * name);
    virtual void on_pdo_message(PDO_MESSAGE * message, int size);
};

#define NUM_MASTER_PARAMS (&LAST_MASTER_COMMAND - &FIRST_MASTER_COMMAND + 1)
