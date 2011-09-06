TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

PROD_HOST += capture_wave monitor_capture
capture_wave_SRCS	+= capture_wave.c
capture_wave_LIBS	+= $(EPICS_BASE_HOST_LIBS)

monitor_capture_SRCS += monitor_capture.c
monitor_capture_LIBS += $(EPICS_BASE_HOST_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

