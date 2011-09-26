TOP=..

include $(TOP)/configure/CONFIG

TARGETS = $(CONFIG_TARGETS)
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

CHECK_RELEASE=YES

include $(TOP)/configure/RULES
