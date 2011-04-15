TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

USR_INCLUDES += -I$(TOP)/.. -I$(TOP)/../../../utils/src
USR_LDFLAGS += -L$(TOP)/.. -L$(TOP)/../../../utils/lib
USR_SYS_LIBS += Scanner xml2 Comrt

#==================================================
# build a support library

LIBRARY_IOC += ecAsyn
ecAsyn_SRCS += ecAsyn.cpp
ecAsyn_LIBS += asyn

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install ec.dbd into <top>/dbd
DBD += ecAsyn.dbd

# specify all source files to be compiled and added to the library
#ec_SRCS += xxx

ec_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC += ectest
ectest_SRCS_DEFAULT += ecAsyn_registerRecordDeviceDriver.cpp ecAsynMain.cpp
ectest_LIBS += ecAsyn asyn
ectest_LIBS += $(EPICS_BASE_IOC_LIBS)


#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

