TOP=../..

include $(TOP)/configure/CONFIG

# LIBRARY

USR_SYS_LIBS += xml2
LIBRARY_IOC += gadc
gadc_SRCS += gadc.cpp
gadc_LIBS += asyn
# DBD += ecAsyn.dbd
INC += gadc.h
# IOC

# PROD_IOC = gadctest
# ethercat_DBD += base.dbd ecAsyn.dbd
# gadctest_SRCS += gadctest.cpp

# These two lines are needed for non-vxWorks builds, such as Linux
# ethercat_SRCS_DEFAULT += ethercatMain.cpp
# ethercat_SRCS_vxWorks += -nil-

# Add locally compiled object code
# ethercat_SRCS +=

# The following adds object code from base/src/vxWorks
# ethercat_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

gadctest_LIBS += gadc asyn
# ethercat_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
