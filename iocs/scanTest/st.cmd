#!../../bin/linux-x86/ethercat
dbLoadDatabase("../../dbd/ecAsyn.dbd")
ethercat_registerRecordDeviceDriver(pdbbase)
ecAsynInit("/tmp/socket", 1000000)

dbLoadRecords("ecApp/Db/MASTER.template", "DEVICE=JRM,PORT=MASTER0,SCAN=I/O Intr")
dbLoadRecords("ecApp/Db/EK1101.template", "DEVICE=JRC,PORT=COUPLER0,SCAN=I/O Intr")
dbLoadRecords("ecApp/Db/EL1004.template", "DEVICE=JRV,PORT=VACUUM0,SCAN=I/O Intr")
dbLoadRecords("ecApp/Db/EL3702.template", "DEVICE=JRR,PORT=RF0,SCAN=I/O Intr")
dbLoadRecords("ecApp/Db/EL2004.template", "DEVICE=JRO,PORT=OUT0,SCAN=I/O Intr")
iocInit()
