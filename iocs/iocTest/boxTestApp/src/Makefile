TOP=../..

include $(TOP)/configure/CONFIG

PROD_IOC_Linux = boxTest

# boxTest.dbd will be installed into <top>/dbd
DBD += boxTest.dbd

# boxTest.dbd will be created from these files
boxTest_DBD += base.dbd
boxTest_DBD += asyn.dbd
boxTest_DBD += ecAsynPortSupport.dbd

# boxTest_registerRecordDeviceDriver.cpp will be created
# boxTest.dbd
boxTest_SRCS += boxTest_registerRecordDeviceDriver.cpp

# These two lines are needed for non-vxWorks builds, such as Linux
boxTest_SRCS_DEFAULT += boxTestMain.cpp
boxTest_SRCS_vxWorks += -nil-

# Add locally compiled object code
#boxTest_SRCS +=

# The following adds object code from base/src/vxWorks
boxTest_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# This line says that this IOC Application depends on the
# xxx Support Module
boxTest_LIBS += asyn
boxTest_LIBS += ethercatSupport

# We need to link this IOC Application against the EPICS Base libraries
boxTest_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
