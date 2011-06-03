TOP=../..

include $(TOP)/configure/CONFIG

# build a program

SHARED_LIBRARIES=NO
STATIC_BUILD=YES

USR_INCLUDES += -I$(ETHERLAB)/include
USR_LDFLAGS += -L$(ETHERLAB)/lib -Wl,-rpath=$(ETHERLAB)/lib
USR_INCLUDES += -I/usr/include/libxml2
USR_SYS_LIBS += ethercat xml2

INC += classes.h parser.h classes.h unpack.h messages.h

LIBRARY += scannerlib
scannerlib_SRCS += parser.c classes.c unpack.c

PROD += scanner
scanner_SRCS += scanner.c
scanner_LIBS += rtutils scannerlib

PROD += slaveinfo
slaveinfo_SRCS += slaveinfo.c

include $(TOP)/configure/RULES
