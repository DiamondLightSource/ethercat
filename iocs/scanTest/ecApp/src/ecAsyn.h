class ecAsyn : public asynPortDriver
{
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    int pdos;
    int devid;
    rtMessageQueueId writeq;
    EC_PDO_ENTRY_MAPPING ** mappings;
    int P_AL_STATE;
    int P_ERROR_FLAG;
public:
    ecAsyn(EC_DEVICE * device, int pdos, rtMessageQueueId writeq, int devid);
    EC_DEVICE * device;
    void on_pdo_message(PDO_MESSAGE * message, int size);
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
    void on_pdo_message(PDO_MESSAGE * message, int size);
};

#define NUM_MASTER_PARAMS (&LAST_MASTER_COMMAND - &FIRST_MASTER_COMMAND + 1)
