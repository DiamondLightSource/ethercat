#!bin/linux-x86/scantest
dbLoadDatabase("dbd/scantest.dbd")
scantest_registerRecordDeviceDriver(pdbbase)
ecAsynInit("/tmp/socket", 1000000)

dbLoadRecords("../../db/NI944.template", "DEVICE=JRTESTNI,PORT=NI0,SCAN=.1 second")
dbLoadRecords("../../db/gadc.template", "DEVICE=JRTESTNI,PORT=NI0,SCAN=.1 second,CHANNEL=4")

iocInit()
