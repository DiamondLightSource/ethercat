TOP=../..

include $(TOP)/configure/CONFIG

# build a program

SHARED_LIBRARIES=NO
STATIC_BUILD=YES

# work-around for ioc builder's benefit:
# the symbol below moved from $(TOP)/configure/RELEASE
ETHERLAB=/dls_sw/work/common/rt/ethercat

USR_INCLUDES += -I$(ETHERLAB)/master
USR_INCLUDES += -I$(ETHERLAB)/include
USR_LDFLAGS += -L$(ETHERLAB)/lib -Wl,-rpath=$(ETHERLAB)/lib
USR_LDFLAGS += -L$(ETHERLAB)/lib64 -Wl,-rpath=$(ETHERLAB)/lib64
USR_INCLUDES += -I/usr/include/libxml2
USR_SYS_LIBS += ethercat xml2

INC += classes.h parser.h classes.h unpack.h messages.h

LIBRARY += scannerlib
scannerlib_SRCS += parser.c classes.c unpack.c

PROD += scanner
scanner_SRCS += scanner.c
scanner_LIBS += rtutils scannerlib Com

PROD += serialtool
serialtool_SRCS += serialtool.c

PROD += slaveinfo
slaveinfo_SRCS += slaveinfo.c

include $(TOP)/configure/RULES
