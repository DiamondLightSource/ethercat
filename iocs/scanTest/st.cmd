#!bin/linux-x86/scantest
dbLoadDatabase("dbd/scantest.dbd")
scantest_registerRecordDeviceDriver(pdbbase)
ecAsynInit("/tmp/socket", 1000000)

dbLoadRecords("../../ethercatApp/Db/MASTER.template", "DEVICE=JRM,PORT=MASTER0,SCAN=I/O Intr")
dbLoadRecords("../../ethercatApp/Db/EK1101.template", "DEVICE=JRC,PORT=COUPLER0,SCAN=I/O Intr")
dbLoadRecords("../../ethercatApp/Db/EL1004.template", "DEVICE=JRV,PORT=VACUUM0,SCAN=I/O Intr")
dbLoadRecords("../../ethercatApp/Db/EL3702.template", "DEVICE=JRR,PORT=RF0,SCAN=I/O Intr")
dbLoadRecords("../../ethercatApp/Db/EL2004.template", "DEVICE=JRO,PORT=OUT0,SCAN=I/O Intr")

iocInit()
