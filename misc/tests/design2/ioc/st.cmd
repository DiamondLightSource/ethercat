#!bin/linux-x86/ectest
dbLoadDatabase("dbd/ecAsyn.dbd")
ecAsyn_registerRecordDeviceDriver(pdbbase)
ecAsynInit

# dlspscInit("PORT0", 0)
# dbLoadRecords("db/test.db")
# iocInit()
