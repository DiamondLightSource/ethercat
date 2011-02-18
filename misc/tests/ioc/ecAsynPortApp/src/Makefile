TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

DBD += ecAsynPort.dbd
DBD += ecAsynPortSupport.dbd

USR_LDFLAGS += -L$(TOP)/../../utils/lib
USR_INCLUDES += -I$(TOP)/../../utils/src
LIBRARY_IOC += ethercatSupport
ethercatSupport_SRCS += ecAsynPort.cpp scanmock.cpp
# buffer.cpp
ethercatSupport_SYS_LIBS += Comrt
ethercatSupport_LIBS += asyn
ethercatSupport_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================

PROD_IOC += testEthercat

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
testEthercat_SRCS_DEFAULT += ecAsynPort_registerRecordDeviceDriver.cpp ecAsynPortDriverMain.cpp
testEthercatVx_SRCS_vxWorks  += ecAsynPort_registerRecordDeviceDriver.cpp
testEthercat_LIBS += ethercatSupport asyn
testEthercat_LIBS += $(EPICS_BASE_IOC_LIBS)

testEthercat_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
