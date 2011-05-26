TOP=../..

include $(TOP)/configure/CONFIG

# build a shared library

INC=rtutils.h msgsock.h
SHARED_LIBRARIES=NO
STATIC_BUILD=YES

LIBRARY += rtutils
rtutils_SRCS += rtutils.c msgsock.c engine.c

include $(TOP)/configure/RULES
