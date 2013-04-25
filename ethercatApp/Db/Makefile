TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# In a Diamond Support Module, install
# templates like this
#
DB := $(DB)  $(filter-out $(DB), $(wildcard ../*.template))

#----------------------------------------------------
# In a Diamond IOC Application, build db files from
# template files like this
#
#DB += BL06I-MO-IOC-01.db
#DB += BL06I-MO-IOC-01_sim.db

#USES_TEMPLATE += $(MOTOR)/db/motor.template
#USES_TEMPLATE += $(MOTOR)/db/basic_motor.template

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

templates:
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL3314 -r 0x120000 -o EL3314.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EK1100 -r  0x110000 -o EK1100.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EK1101 -r  0x110000 -o EK1101.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d  EK1122 -r 0x110000 -o EK1122.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL1004 -r 0x100000 -o EL1004.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL1014 -r 0x100000 -o EL1014.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL1124 -r 0x100000 -o EL1124.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL1502 -r 0x120000 -o EL1502.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL2004 -r 0x100000 -o EL2004.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL2024-0010 -r 0x10000a -o EL2024-0010.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL2124 -r 0x100000 -o EL2124.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL2624 -r 0x100000 -o EL2624.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL3104 -r 0x100000 -o EL3104.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL3124 -r 0x110000 -o EL3124.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL3162 -r 0x110000 -o EL3162.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL3201 -r 0x100000 -o EL3201.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL3202-0010 -r 0x11000a -o EL3202-0010.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL3314 -r 0x120000 -o EL3314.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL3702 -r 0x20000 -o EL3702.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL4134 -r 0x3f90000 -o EL4134.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL4732 -r 0x30000 -o EL4732.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL9410 -r 0x100000 -o EL9410.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL9505 -r 0x100000 -o EL9505.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL9512 -r 0x100000 -o EL9512.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EP1018-0002 -r 0x110002 -o EP1018-0002.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EP1122-0001 -r 0x110001 -o EP1122-0001.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EP2338-0002 -r 0x120002 -o EP2338-0002.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EP2624-0002 -r 0x110002 -o EP2624-0002.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EP3174-0002 -r 0x120002 -o EP3174-0002.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EP3204-0002 -r 0x120002 -o EP3204-0002.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EP3314-0002 -r 0x100002 -o EP3314-0002.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EP4174-0002 -r 0x110002 -o EP4174-0002.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EP4374-0002 -r 0x110002 -o EP4374-0002.template
	../../etc/scripts/maketemplate.py -b ../../etc/xml -d EL3602 -r 0x100000 -o EL3602.template
