#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <epicsThread.h>
#include <epicsExport.h>
#include <asynPortDriver.h>
#include <iocsh.h>

#include "ecAsyn.h"

extern "C"
{
#include "classes.h"
#include "parser.h"
}

ecAsyn::ecAsyn(const char * name, int testmode) : 
    asynPortDriver(name,
                   1, /* maxAddr */
                   1000 + 16, /* max parameters */
                   asynInt32Mask | asynDrvUserMask, /* interface mask*/
                   asynInt32Mask, /* interrupt mask */
                   0, /* non-blocking, no addresses */
                   1, /* autoconnect */
                   0, /* default priority */
                   0), /* default stack size */
    testmode(testmode)
{
    printf("ecAsyn INIT %s %d\n", name, testmode);

    createParam("FULL_TIMEOUTS",   asynParamInt32, &P_FullTimeouts);
    createParam("EMPTY_TIMEOUTS",  asynParamInt32, &P_EmptyTimeouts);
    createParam("PROTOCOL_ERRORS", asynParamInt32, &P_ProtocolErrors);
    createParam("READ_SIGNALS",    asynParamInt32, &P_ReadSignals);
    createParam("READ_COMMS",      asynParamInt32, &P_ReadComms);
    createParam("READ_FLASH",      asynParamInt32, &P_ReadFlash);
    createParam("WRITE_FLASH",     asynParamInt32, &P_WriteFlash);
    createParam("SOFT_RESET",      asynParamInt32, &P_SoftReset);

}

/*
asynStatus ecAsyn::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    unlock();
    int ok = writeCommand(pasynUser->reason, value);
    lock();
    if(ok)
    {
        return asynPortDriver::writeInt32(pasynUser, value);
    }
    else
    {
        return asynError;
    }
}
*/
 /*
void SoftReset(FIFO * fifo)
{
    printf("soft reset\n");
    fifo->mem->fpga_ver = 0;
    epicsThreadSleep(5.0);
}

int ecAsyn::writeCommand(int function, epicsInt32 value)
{
    flash_parameters_t flash_parameters;
    communication_registers_t communication_registers;
    monitored_signals_t monitored_signals;
    fifo->error = 0;
    if(function == P_ReadSignals)
    {
        if(SignalsRead(fifo, &monitored_signals))
        {
            lock();
            set_param_monitored_signals(this, &monitored_signals);
            unlock();
        }
    }
    else if(function == P_ReadComms)
    {
        if(CommsRead(fifo, &communication_registers))
        {
            lock();
            set_param_communication_registers(this, &communication_registers);
            unlock();
        }
    }
    else if(function == P_ReadFlash)
    {
        if(FlashRead(fifo, &flash_parameters))
        {
            lock();
            set_param_flash_parameters(this, &flash_parameters);
            unlock();
        }
    }
    else if(function == P_WriteFlash)
    {
        lock();
        get_param_flash_parameters(this, &flash_parameters);
        unlock();
        FlashWrite(fifo, &flash_parameters);
    }
    else if(function >= FIRST_communication_registers_COMMAND &&
            function <= LAST_communication_registers_COMMAND)
    {
        int ofs = function - FIRST_communication_registers_COMMAND;
        CommsUpdate(fifo, ofs, value);
    }
    else if(function == P_SoftReset)
    {
        SoftReset(fifo);
    }
    lock();
    setIntegerParam(P_FullTimeouts, fifo->full_timeouts);
    setIntegerParam(P_EmptyTimeouts, fifo->empty_timeouts);
    setIntegerParam(P_ProtocolErrors, fifo->protocol_errors);
    unlock();
    // reset FIFO on error
    if(fifo->error)
    {
        SoftReset(fifo);
        fifo->error = 0;
        return 0;
    }
    else
    {
        return 1;
    }
}
*/

extern "C"
{
    /* EPICS iocsh shell commands */
    static const iocshArg InitArg[] = { { "name",     iocshArgString },
                                        { "testmode", iocshArgInt    } };
    static const iocshArg * const InitArgs[] = { &InitArg[0], &InitArg[1] };
    static const iocshFuncDef initFuncDef = {"ecAsynInit", 2, InitArgs};
    static void initCallFunc(const iocshArgBuf * args)
    {
        char * buffer = load_config("scanner.xml");
        EC_CONFIG * config = (EC_CONFIG *)calloc(1, sizeof(EC_CONFIG));
        read_config2(buffer, strlen(buffer), config);
        //new ecAsyn(args[0].sval, args[1].ival);
    }
    void ecAsynRegistrar(void)
    {
        iocshRegister(&initFuncDef,initCallFunc);
    }
    epicsExportRegistrar(ecAsynRegistrar);
}
