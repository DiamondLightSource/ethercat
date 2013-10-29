TOP=../..

include $(TOP)/configure/CONFIG

# ------------------------
# Build an IOC Application
# ------------------------

PROD_IOC = es3602test

# es3602test.dbd will be installed into <top>/dbd
DBD += es3602test.dbd

# es3602test.dbd will be created from these files
es3602test_DBD += base.dbd
es3602test_DBD += asyn.dbd
es3602test_DBD += ecAsynPortSupport.dbd
#es3602test_DBD += xxx.dbd

# es3602test_registerRecordDeviceDriver.cpp will be created
# es3602test.dbd
es3602test_SRCS += es3602test_registerRecordDeviceDriver.cpp

# These two lines are needed for non-vxWorks builds, such as Linux
es3602test_SRCS_DEFAULT += es3602testMain.cpp
es3602test_SRCS_vxWorks += -nil-

# Add locally compiled object code
#es3602test_SRCS +=

# The following adds object code from base/src/vxWorks
es3602test_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# This line says that this IOC Application depends on the
# xxx Support Module
#es3602test_LIBS += xxx

# We need to link this IOC Application against the EPICS Base libraries
es3602test_LIBS += asyn
es3602test_LIBS += ethercatSupport
es3602test_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
