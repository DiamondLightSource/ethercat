TOP=../..

include $(TOP)/configure/CONFIG

# ------------------------
# Build an IOC Application
# ------------------------

PROD_IOC = i12test

# i12test.dbd will be installed into <top>/dbd
DBD += i12test.dbd

# i12test.dbd will be created from these files
i12test_DBD += base.dbd
i12test_DBD += asyn.dbd
i12test_DBD += ecAsyn.dbd
#i12test_DBD += xxx.dbd

# i12test_registerRecordDeviceDriver.cpp will be created
# i12test.dbd
i12test_SRCS += i12test_registerRecordDeviceDriver.cpp

# These two lines are needed for non-vxWorks builds, such as Linux
i12test_SRCS_DEFAULT += i12testMain.cpp
i12test_SRCS_vxWorks += -nil-

# Add locally compiled object code
#i12test_SRCS +=

# The following adds object code from base/src/vxWorks
i12test_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# This line says that this IOC Application depends on the
# xxx Support Module
#i12test_LIBS += ethercatSupport asyn

# We need to link this IOC Application against the EPICS Base libraries
i12test_LIBS += ecAsyn
i12test_LIBS += asyn
i12test_LIBS += $(EPICS_BASE_IOC_LIBS)

DATA += i12test-gui
DATA += i12test.grp
DATA += scanner.xml

include $(TOP)/configure/RULES

scanner.xml: ../chain.xml
	$(ETHERCAT)/etc/scripts/expandChain.py $(ETHERCAT)/etc/xml ../chain.xml > scanner.xml

