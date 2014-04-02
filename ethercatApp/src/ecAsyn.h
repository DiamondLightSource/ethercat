#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <asynPortDriver.h>
#include <ellLib.h>
#include "classes.h" // EC_PDO_ENTRY_MAPPING, EC_DEVICE

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
#define ECALStateString     "AL_STATE"      /**< (asynInt32, r/o) slave state */
#define ECErrorFlagString   "ERROR_FLAG"    /**< (asynInt32, r/o) slave's error flag */
#define ECDisableString     "DISABLE"       /**< (asynInt32, r/o) slave's disabled flag */ 
#define ECDeviceTypename    "DEV_TYPENAME"  /**< (asynOctect, r/o) device type */
#define ECDeviceRevision    "DEV_REVISION"  /**< (asynoctect, r/o) device revision */
#define ECDevicePosition    "DEV_POSITION"  /**< (asynOctect, r/o) slave's position */
#define ECDeviceName        "DEV_NAME"      /**< (asynOctec, r/o) slave's asyn port name */

class ecAsyn : public asynPortDriver, public ProcessDataObserver
{
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    int pdos;
    int devid;
    rtMessageQueueId writeq;
    EC_PDO_ENTRY_MAPPING ** mappings;
    int P_AL_STATE;
#define FIRST_SLAVE_COMMAND P_AL_STATE
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
    ecAsyn(EC_DEVICE * device, int pdos, ENGINE_USER * usr, int devid);
    EC_DEVICE * device;
    virtual void on_pdo_message(PDO_MESSAGE * message, int size);
    virtual asynStatus getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high);
    virtual asynStatus getBoundsForMapping(struct EC_PDO_ENTRY_MAPPING *m, epicsInt32 *low, epicsInt32 *high);
};

#define NUM_SLAVE_PARAMS (&LAST_SLAVE_COMMAND - &FIRST_SLAVE_COMMAND + 1)

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
