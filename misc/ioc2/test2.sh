#!../ioc/bin/linux-x86/testEthercat

dbLoadDatabase("../ioc/dbd/ecAsynPort.dbd")
ecAsynPort_registerRecordDeviceDriver(pdbbase)
ecConfigure("ECAT1",6,6)
dbLoadRecords("test2.db")
enableWriterMsg(0)
#enableWriterMsg(0)
enableReaderMsg(0)
asynSetTraceMask("ECAT1", 0, 0x00)
asynSetTraceIOMask("ECAT1", 0, 0x00)
#asynSetTraceMask("ECAT0", 0,0)
iocInit()



