class ecAsyn : public asynPortDriver
{
    int testmode;
    //int writeCommand(int function, epicsInt32 value = 0);
    //virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);

    int P_FullTimeouts;
    int P_EmptyTimeouts;
    int P_ProtocolErrors;
    int P_ReadSignals;
    int P_ReadComms;
    int P_ReadFlash;
    int P_WriteFlash;
    int P_SoftReset;

public:
    
    ecAsyn(const char * name, int dummy);

};
