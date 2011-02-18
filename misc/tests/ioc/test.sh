#!bin/linux-x86/testEthercat

dbLoadDatabase("dbd/ecAsynPort.dbd")
ecAsynPort_registerRecordDeviceDriver(pdbbase)

# ethercat scanner port
ecConfigure("ECAT0", "/tmp/scanner.sock", 100, 1)

# circular buffer port
accConfigure("ECAT1", "ECAT0", 400, "value0", 100, 50)

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
