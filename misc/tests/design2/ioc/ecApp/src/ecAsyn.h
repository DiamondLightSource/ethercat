class ecAsyn : public asynPortDriver
{
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    rtMessageQueueId writeq;
    EC_PDO_ENTRY_MAPPING ** mappings;
public:
    ecAsyn(EC_DEVICE * device, int pdos, rtMessageQueueId writeq);
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
