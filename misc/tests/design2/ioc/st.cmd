#!bin/linux-x86/ectest
dbLoadDatabase("dbd/ecAsyn.dbd")
ecAsyn_registerRecordDeviceDriver(pdbbase)
ecAsynInit("/tmp/socket", 1000000)

dbLoadRecords("test.db", "DEVICE=JR")
iocInit()
