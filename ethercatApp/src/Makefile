TOP=../..

include $(TOP)/configure/CONFIG

# LIBRARY

USR_SYS_LIBS += xml2
LIBRARY_IOC += ecAsyn
ecAsyn_SRCS += ecAsyn.cpp
ecAsyn_LIBS += asyn scannerlib rtutils
DBD += ecAsyn.dbd

# IOC

PROD_IOC = ethercat
ethercat_DBD += base.dbd ecAsyn.dbd
ethercat_SRCS += ethercat_registerRecordDeviceDriver.cpp

# These two lines are needed for non-vxWorks builds, such as Linux
ethercat_SRCS_DEFAULT += ethercatMain.cpp
ethercat_SRCS_vxWorks += -nil-

# Add locally compiled object code
# ethercat_SRCS +=

# The following adds object code from base/src/vxWorks
ethercat_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

ethercat_LIBS += ecAsyn asyn
ethercat_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
