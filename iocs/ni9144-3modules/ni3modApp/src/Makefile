TOP=../..

include $(TOP)/configure/CONFIG

# ------------------------
# Build an IOC Application
# ------------------------

PROD_IOC = ni3mod
#PROD_HOST += timeTest

#timeTest_SRCS += test.c

# ni3mod.dbd will be installed into <top>/dbd
DBD += ni3mod.dbd

# ni3mod.dbd will be created from these files
ni3mod_DBD += base.dbd
ni3mod_DBD += asyn.dbd
ni3mod_DBD += ecAsyn.dbd

# ni3mod_registerRecordDeviceDriver.cpp will be created
# ni3mod.dbd
ni3mod_SRCS += ni3mod_registerRecordDeviceDriver.cpp

# These two lines are needed for non-vxWorks builds, such as Linux
ni3mod_SRCS_DEFAULT += ni3modMain.cpp
ni3mod_SRCS_vxWorks += -nil-

# Add locally compiled object code
#ni3mod_SRCS +=

# The following adds object code from base/src/vxWorks
ni3mod_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

ni3mod_LIBS += ecAsyn
ni3mod_LIBS += asyn
ni3mod_LIBS += $(EPICS_BASE_IOC_LIBS)

DATA += scanner.xml
DATA += scanner-analog-output.xml

include $(TOP)/configure/RULES

scanner.xml: ../chain.xml
	echo "Executing rule to create scanner.xml"
	$(ETHERCAT)/etc/scripts/expandChain.py $(ETHERCAT)/etc/xml $< > $@

scanner-analog-output.xml: ../chain-analog-output.xml
	echo "Executing rule to create scanner-analog-output.xml"
	$(ETHERCAT)/etc/scripts/expandChain.py $(ETHERCAT)/etc/xml ../chain-analog-output.xml > $@

