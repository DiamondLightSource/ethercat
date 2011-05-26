TOP=../..

include $(TOP)/configure/CONFIG

# IOC

DBD += scantest.dbd
scantest_DBD += base.dbd ecAsyn.dbd

PROD_IOC = scantest
scantest_SRCS += scantest_registerRecordDeviceDriver.cpp

# These two lines are needed for non-vxWorks builds, such as Linux
scantest_SRCS_DEFAULT += scantestMain.cpp
scantest_SRCS_vxWorks += -nil-

# Add locally compiled object code
# scantest_SRCS +=

# The following adds object code from base/src/vxWorks
scantest_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

scantest_LIBS += ecAsyn asyn
scantest_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
