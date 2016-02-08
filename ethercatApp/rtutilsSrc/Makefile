TOP=../..

include $(TOP)/configure/CONFIG

# build a shared library

INC=rtutils.h msgsock.h
SHARED_LIBRARIES=NO
STATIC_BUILD=YES


LIBRARY += rtutils
rtutils_SRCS += rtutils.c
rtutils_SRCS += msgsock.c
rtutils_SRCS += engine.c

PROD_IOC=rthreadtest
rthreadtest_LIBS += rtutils

PROD_IOC=enginetest
enginetest_LIBS += rtutils

include $(TOP)/configure/RULES
