#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <asynPortDriver.h>
#include <ellLib.h>
#include "classes.h" // EC_PDO_ENTRY_MAPPING, EC_DEVICE
#include "messages.h" //PDO_MESSAGE
#include "rtutils.h"  //rtMessageQueueId

struct ENGINE_USER;

template <class T> struct ListNode
{
    ELLNODE node;
};

class ProcessDataObserver : public ListNode<ProcessDataObserver>
{
public:
    virtual void on_pdo_message(PDO_MESSAGE * message, int size) = 0;
    virtual ~ProcessDataObserver() {}
};

class ecAsyn;
class ecSdoAsyn;
class XFCPort : public asynPortDriver
{
    int P_Missed;
    ecAsyn *parentPort;
    struct EC_PDO_ENTRY_MAPPING *mapping;
public:
    void incMissed()
    {
        epicsInt32 value;
        getIntegerParam(P_Missed, &value);
        value = value + 1;
        if(value == INT32_MAX)
        {
            value = 0;
        }
        setIntegerParam(P_Missed, value);
        callParamCallbacks();
    }
    XFCPort(const char * name);
};

/** Strings defining parameters for a slave port
 */
#define ECSlaveInfoString   "SLAVE_INFO"    /**< (asynInt32, r/o) slave info result */
#define ECALStateString     "AL_STATE"      /**< (asynInt32, r/o) slave state */
#define ECErrorFlagString   "ERROR_FLAG"    /**< (asynInt32, r/o) slave's error flag */
#define ECDisableString     "DISABLE"       /**< (asynInt32, r/o) slave's disabled flag */ 
#define ECDeviceTypename    "DEV_TYPENAME"  /**< (asynOctect, r/o) device type */
#define ECDeviceRevision    "DEV_REVISION"  /**< (asynoctect, r/o) device revision */
#define ECDevicePosition    "DEV_POSITION"  /**< (asynOctect, r/o) slave's position */
#define ECDeviceName        "DEV_NAME"      /**< (asynOctec, r/o) slave's asyn port name */

typedef struct sdo_paramrecord 
{
    int param_val;              /* param in asyn with value */
    int param_stat;             /* param in asyn with request state */
    int param_trig;             /* param in asyn to trigger a read req */
    EC_SDO_ENTRY *sdoentry;
    // TODO: review - maybe redundant?
    ecAsyn * ecasyn_port;
    ecSdoAsyn * ecsdoasyn_port;
} sdo_paramrecord_t;


class ecAsyn : public asynPortDriver, public ProcessDataObserver
{
private:
    //disallow evil constructors
    ecAsyn(const ecAsyn&);
    void operator=(const ecAsyn&);
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    int pdos;
    int devid;
    EC_PDO_ENTRY_MAPPING ** mappings;
    int P_SLAVEINFO;
#define FIRST_SLAVE_COMMAND P_SLAVEINFO
    int P_AL_STATE;
    int P_ERROR_FLAG;
    int P_DISABLE;
    int P_DEVTYPENAME;
    int P_DEVREVISION;
    int P_DEVPOSITION;
    int P_DEVNAME;
#define LAST_SLAVE_COMMAND P_DEVNAME
    int P_First_PDO;
    int P_Last_PDO;
public:
    ecAsyn(EC_DEVICE * device, int pdos, int sdos, ENGINE_USER * usr, int devid);
    int sdos;
    rtMessageQueueId writeq;
    EC_DEVICE * device;
    virtual void on_pdo_message(PDO_MESSAGE * message, int size);
    virtual asynStatus getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high);
    virtual asynStatus getBoundsForMapping(struct EC_PDO_ENTRY_MAPPING *m, epicsInt32 *low, epicsInt32 *high);
};

#define NUM_SLAVE_PARAMS (&LAST_SLAVE_COMMAND - &FIRST_SLAVE_COMMAND + 1)

class ecSdoAsyn : public asynPortDriver, public ListNode<int>
{
private:
    //disallow evil constructors
    ecSdoAsyn(const ecSdoAsyn&);
    void operator=(const ecSdoAsyn&);
    sdo_paramrecord_t ** paramrecords;
    inline int firstParam() { return paramrecords[0]->param_val;}
    inline int lastParam() {return paramrecords[parent->sdos-1]->param_trig;}
    bool rangeOkay(int param);
    bool isTrig(int param);
    bool isVal(int param);
    bool isStat(int param);
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
public:    
    ecAsyn * parent;
    ecSdoAsyn(char *sdoport, ecAsyn *p);
    void on_sdo_message(SDO_READ_MESSAGE * msg, int size);
    EC_SDO_ENTRY * getSdoentry(int param);
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
