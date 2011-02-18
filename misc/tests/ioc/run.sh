#!bin/linux-x86/testEthercat

dbLoadDatabase("dbd/ecAsynPort.dbd")
ecAsynPort_registerRecordDeviceDriver(pdbbase)

# ethercat scanner port (portname, socketname, monitor period, selftest)
ecConfigure("ECAT0", "/tmp/scanner.sock", 1000, 0)

# circular buffer port (portname, master, address, command, buffersize, triggerdelay, period)
# address is (parameter,monitor period)
accConfigure("ECAT2", "ECAT0", 300, "value0,1", 10000, 5000, 10000)

#asynSetTraceIOMask("ECAT0", 100, 0xffff)
#asynSetTraceMask("ECAT0", 100, 0xffff)
#asynSetTraceIOMask("ECAT0", 200, 0xffff)
#asynSetTraceMask("ECAT0", 200, 0xffff)
#asynSetTraceIOMask("ECAT0", 400, 0xffff)
#asynSetTraceMask("ECAT0", 400, 0xffff)
#asynSetTraceIOMask("ECAT0", 400, 0xffff)
#asynSetTraceMask("ECAT0", 400, 0xffff)

dbLoadRecords("test.db")
iocInit()
