#!bin/linux-x86/testEthercat

dbLoadDatabase("dbd/ecAsynPort.dbd")
ecAsynPort_registerRecordDeviceDriver(pdbbase)
ecConfigure("ECAT0",7,10)
dbLoadRecords("test3.db")
enableWriterMsg(0)
#enableWriterMsg(0)
enableReaderMsg(0)
asynSetTraceMask("ECAT0", 0, 0x00)
asynSetTraceIOMask("ECAT0", 0, 0x00)
#asynSetTraceMask("ECAT0", 0,0)
iocInit()



