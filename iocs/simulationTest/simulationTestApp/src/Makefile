TOP=../..

include $(TOP)/configure/CONFIG

# ------------------------
# Build an IOC Application
# ------------------------

PROD_IOC = simulationTest

# simulationTest.dbd will be installed into <top>/dbd
DBD += simulationTest.dbd

# simulationTest.dbd will be created from these files
simulationTest_DBD += base.dbd
simulationTest_DBD += ecAsyn.dbd

# simulationTest_registerRecordDeviceDriver.cpp will be created
# simulationTest.dbd
simulationTest_SRCS += simulationTest_registerRecordDeviceDriver.cpp

# These two lines are needed for non-vxWorks builds, such as Linux
simulationTest_SRCS_DEFAULT += simulationTestMain.cpp
simulationTest_SRCS_vxWorks += -nil-

# Add locally compiled object code
#simulationTest_SRCS +=

# The following adds object code from base/src/vxWorks
simulationTest_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

simulationTest_LIBS += ecAsyn
simulationTest_LIBS += asyn

# We need to link this IOC Application against the EPICS Base libraries
simulationTest_LIBS += $(EPICS_BASE_IOC_LIBS)


include $(TOP)/configure/RULES
