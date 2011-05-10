TOP=../../../..

include $(TOP)/configure/CONFIG

PROD_IOC += ectest
ectest_SRCS_DEFAULT += ecAsyn_registerRecordDeviceDriver.cpp ecAsynMain.cpp
ectest_LIBS += ecAsyn asyn
ectest_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES

