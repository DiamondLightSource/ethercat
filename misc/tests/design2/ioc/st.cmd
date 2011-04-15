#!bin/linux-x86/ectest
dbLoadDatabase("dbd/ecAsyn.dbd")
ecAsyn_registerRecordDeviceDriver(pdbbase)
ecAsynInit("/tmp/socket", 1000000)

dbLoadRecords("ecApp/Db/MASTER.template", "DEVICE=JR,PORT=MASTER0,SCAN=I/O Intr")
dbLoadRecords("ecApp/Db/EK1101.template", "DEVICE=JR,PORT=COUPLER0,SCAN=I/O Intr")
dbLoadRecords("ecApp/Db/EL1004.template", "DEVICE=JR,PORT=VACUUM0,SCAN=I/O Intr")
dbLoadRecords("ecApp/Db/EL3702.template", "DEVICE=JR,PORT=RF0,SCAN=I/O Intr")
dbLoadRecords("ecApp/Db/EL2004.template", "DEVICE=JO,PORT=OUT0,SCAN=I/O Intr")
iocInit()
