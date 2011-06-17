TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

DB += NI_9144r1.template
DB += NI_9144r3.template
DB += ni3wave.db
DB += ni9234.db

#----------------------------------------------------
# In a Diamond IOC Application, build db files from
# template files like this
#
DB += ni3mod-test.db

#USES_TEMPLATE += $(MOTOR)/db/motor.template
#USES_TEMPLATE += $(MOTOR)/db/basic_motor.template

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

../O.Common/%.template: ../%.template
	cp $< $@

../NI_9144r1.template:
	$(ETHERCAT)/etc/scripts/maketemplate.py -b $(ETHERCAT)/etc/xml -d "NI 9144" -r 0x00000001 -o $@

../NI_9144r3.template:
	$(ETHERCAT)/etc/scripts/maketemplate.py -b $(ETHERCAT)/etc/xml -d "NI 9144" -r 0x00000003 -o $@

../EL4732.template:
	$(ETHERCAT)/etc/scripts/maketemplate.py -b $(ETHERCAT)/etc/xml -d "EL4732" -r 0x00030000 -o $@ 

../EL4134.template:
	$(ETHERCAT)/etc/scripts/maketemplate.py -b $(ETHERCAT)/etc/xml -d "EL4134" -r 0x03f80000 -o $@ 
