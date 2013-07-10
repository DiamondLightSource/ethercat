TOP=../..

include $(TOP)/configure/CONFIG

# build a program

SHARED_LIBRARIES=NO
STATIC_BUILD=YES

# work-around for ioc builder's benefit:
# the symbol below moved from $(TOP)/configure/RELEASE
VERSION=1-5-2dls3
ETHERLAB=/dls_sw/prod/tools/RHEL6-x86_64/ethercat/$(VERSION)
ETHERLABPREFIX=$(ETHERLAB)/prefix


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

PROD += serialtool
# need private header files for the serial number write IOCTL
serialtool_INCLUDES += -I$(ETHERLAB)/src/ethercat-$(subst -,.,$(VERSION))/master
serialtool_SRCS += serialtool.c

PROD += slaveinfo
slaveinfo_SRCS += slaveinfo.c

include $(TOP)/configure/RULES
