#!bin/linux-x86/testEthercat

dbLoadDatabase("dbd/ecAsynPort.dbd")
ecAsynPort_registerRecordDeviceDriver(pdbbase)
enableReaderMsg(0)
enableWriterMsg(0)
ecConfigure("ECAT0",1,5)
dbLoadRecords("test.db")
#enableWriterMsg(0)
asynSetTraceMask("ECAT0", 0, 0x00)
asynSetTraceIOMask("ECAT0", 0, 0x00)
#asynSetTraceMask("ECAT0", 0,0)
iocInit()



