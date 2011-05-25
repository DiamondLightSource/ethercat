#!bin/linux-x86/scantest

epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES","10000000")

dbLoadDatabase("dbd/scantest.dbd")
scantest_registerRecordDeviceDriver(pdbbase)
ecAsynInit("/tmp/socket", 1000000)

dbLoadRecords("../../db/NI944.template", "DEVICE=JRTESTNI,PORT=NI0,SCAN=.1 second")
dbLoadRecords("../../db/gadc.template", "DEVICE=JRTESTNI,PORT=NI0,SCAN=.1 second,CHANNEL=4,SAMPLES=1000000")

iocInit()
