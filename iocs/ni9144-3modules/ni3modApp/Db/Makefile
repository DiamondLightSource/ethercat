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
#DB += motor.template
#DB += basic_motor.template
DB += NI_9144.template
DB += EL3201.template
DB += ni3wave.db
DB += ni9234.db

#----------------------------------------------------
# In a Diamond IOC Application, build db files from
# template files like this
#
#DB += BL06I-MO-IOC-01.db
DB += ni3mod-test.db

#USES_TEMPLATE += $(MOTOR)/db/motor.template
#USES_TEMPLATE += $(MOTOR)/db/basic_motor.template

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

../O.Common/EL3201.template: ../EL3201.template
	cp ../EL3201.template ../O.Common/EL3201.template 

../EL3201.template:
	$(ETHERCAT)/etc/scripts/maketemplate.py -b $(ETHERCAT)/etc/xml \
    -d "EL3201" -r 0x00100000 -o ../EL3201.template

../O.Common/NI_9144.template: ../NI_9144.template
	cp ../NI_9144.template ../O.Common/NI_9144.template 

../NI_9144.template:
	$(ETHERCAT)/etc/scripts/maketemplate.py -b $(ETHERCAT)/etc/xml -d "NI 9144" -r 0x00000003 -o ../NI_9144.template



