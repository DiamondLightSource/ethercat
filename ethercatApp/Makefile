TOP = ..
include $(TOP)/configure/CONFIG
DIRS := rtutilsSrc scannerSrc 
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *src*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *Db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *opi*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard protocol))
include $(TOP)/configure/RULES_DIRS

