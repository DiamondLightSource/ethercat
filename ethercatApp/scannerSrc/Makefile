TOP=../..

include $(TOP)/configure/CONFIG

# build a program

SHARED_LIBRARIES=NO
STATIC_BUILD=YES

# work-around for ioc builder's benefit:
# the symbol below moved from $(TOP)/configure/RELEASE
ETHERLAB=/dls_sw/prod/R3.14.11/support/etherlab/1-5-pre
ETHERLABPREFIX=$(ETHERLAB)/opt/$(EPICS_HOST_ARCH)/etherlab

# need private header files for the serial number write IOCTL
USR_INCLUDES += -I$(ETHERLAB)/src/ethercat-1.5-r1977/master
USR_INCLUDES += -I$(ETHERLABPREFIX)/include
USR_LDFLAGS += -L$(ETHERLABPREFIX)/lib -Wl,-rpath=$(ETHERLABPREFIX)/lib
USR_INCLUDES += -I/usr/include/libxml2
USR_SYS_LIBS += ethercat xml2

INC += classes.h parser.h classes.h unpack.h messages.h

LIBRARY += scannerlib
scannerlib_SRCS += parser.c classes.c unpack.c

PROD += scanner
scanner_SRCS += scanner.c simulation.c
scanner_LIBS += rtutils scannerlib Com

#PROD += expand
#expand_SRCS += expand.c
#expnad_LIBS += scannerlib

PROD += serialtool
serialtool_SRCS += serialtool.c

PROD += parserTest
parserTest_SRCS += parserTest.c 
parserTest_LIBS += scannerlib Com

PROD += slaveinfo
slaveinfo_SRCS += slaveinfo.c

include $(TOP)/configure/RULES
