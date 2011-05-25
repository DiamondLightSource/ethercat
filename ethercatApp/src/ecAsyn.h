class pdo_listener
{
public:
    virtual void on_pdo_message(PDO_MESSAGE * message, int size) = 0;
    virtual ~pdo_listener() {}
};

extern LIST * ethercat_pdo_listeners;

struct PORT_NODE
{
    NODE node;
    pdo_listener * port;
};

class Ringbuffer;

class ecAsyn : public asynPortDriver, public pdo_listener
{
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 * value);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                      size_t nElements, size_t *nIn);
    int pdos;
    int devid;
    rtMessageQueueId writeq;
    EC_PDO_ENTRY_MAPPING ** mappings;
    int P_AL_STATE;
    int P_ERROR_FLAG;
    int P_Trigger;
    int P_Offset;
    int P_Samples;
    ELLLIST samplers;

public:
    ecAsyn(EC_DEVICE * device, int pdos, rtMessageQueueId writeq, int devid);
    EC_DEVICE * device;
    virtual void on_pdo_message(PDO_MESSAGE * message, int size);
};

class ecMaster : public asynPortDriver
{
    int P_Cycle;
#define FIRST_MASTER_COMMAND P_Cycle
    int P_WorkingCounter;
    int P_WcState;
#define LAST_MASTER_COMMAND P_WcState
public:
    ecMaster(char * name);
    virtual void on_pdo_message(PDO_MESSAGE * message, int size);
};

#define NUM_MASTER_PARAMS (&LAST_MASTER_COMMAND - &FIRST_MASTER_COMMAND + 1)
