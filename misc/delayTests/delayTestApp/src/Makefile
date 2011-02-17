TOP=../..

include $(TOP)/configure/CONFIG


# ------------------------
# Build an IOC Application
# ------------------------

PROD_IOC_Linux = delayTest

# delayTest.dbd will be installed into <top>/dbd
DBD += delayTest.dbd

# delayTest.dbd will be created from these files
delayTest_DBD += base.dbd
delayTest_DBD += asyn.dbd
delayTest_DBD += ecAsynPortSupport.dbd
#delayTest_DBD += xxx.dbd

# delayTest_registerRecordDeviceDriver.cpp will be created
# delayTest.dbd
delayTest_SRCS += delayTest_registerRecordDeviceDriver.cpp

# These two lines are needed for non-vxWorks builds, such as Linux
delayTest_SRCS_DEFAULT += delayTestMain.cpp
delayTest_SRCS_vxWorks += -nil-

# Add locally compiled object code
#delayTest_SRCS +=

# The following adds object code from base/src/vxWorks
delayTest_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# This line says that this IOC Application depends on the
# xxx Support Module
#delayTest_LIBS += xxx

# We need to link this IOC Application against the EPICS Base libraries
delayTest_LIBS += asyn
delayTest_LIBS += ethercatSupport
delayTest_LIBS += $(EPICS_BASE_IOC_LIBS)

# ---------------------------------------------------

# NOTE: To build SNL programs, SNCSEQ must be defined
# in the <top>/configure/RELEASE file
#
# The difference between a file with an "st" extension
# and one with an "stt" extension is:
# The "st" extension files get passed through the C preprocessor
# whereas the "stt" files do not.

#ifneq ($(SNCSEQ),)
    # This builds sncExample as a component of delayTest
#    delayTest_SNCFLAGS += +r
#    delayTest_DBD      += sncExample.dbd
#    delayTest_SRCS     += sncExample.stt
#    delayTest_LIBS     += seq pv

    # The following builds sncExample as a standalone application
#    PROD_HOST           += sncExample
#    sncExample_SNCFLAGS += +m
#    sncExample_SRCS     += sncExample.stt
#    sncExample_LIBS     += seq pv
    # Here, we need to specify the dependency on EPICS Base libraries
    # again because this is a standalone application and
    # not part of delayTest
#    sncExample_LIBS     += $(EPICS_BASE_HOST_LIBS)
#endif

include $(TOP)/configure/RULES
