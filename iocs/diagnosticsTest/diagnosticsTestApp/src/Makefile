TOP=../..

include $(TOP)/configure/CONFIG

# ------------------------
# Build an IOC Application
# ------------------------

PROD_IOC = diagnosticsTest

# diagnosticsTest.dbd will be installed into <top>/dbd
DBD += diagnosticsTest.dbd

# diagnosticsTest.dbd will be created from these files
diagnosticsTest_DBD += base.dbd
diagnosticsTest_DBD += ecAsyn.dbd

# diagnosticsTest_registerRecordDeviceDriver.cpp will be created
# diagnosticsTest.dbd
diagnosticsTest_SRCS += diagnosticsTest_registerRecordDeviceDriver.cpp

# These two lines are needed for non-vxWorks builds, such as Linux
diagnosticsTest_SRCS_DEFAULT += diagnosticsTestMain.cpp
diagnosticsTest_SRCS_vxWorks += -nil-

# Add locally compiled object code
#diagnosticsTest_SRCS +=

# The following adds object code from base/src/vxWorks
diagnosticsTest_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

diagnosticsTest_LIBS += ecAsyn
diagnosticsTest_LIBS += asyn

# We need to link this IOC Application against the EPICS Base libraries
diagnosticsTest_LIBS += $(EPICS_BASE_IOC_LIBS)


include $(TOP)/configure/RULES

