TOP = ../..
include $(TOP)/configure/CONFIG

# include the list of IOCs
include $(TOP)/etc/makeIocs/Makefile.iocs

# this is the dls-xml-iocbuilder.py file
XMLBUILDER := dls-xml-iocbuilder.py
#XMLBUILDER := /dls_sw/work/common/python/iocbuilder/xmlbuilder/xmlbuilder.py

# set to -d to get debugging
DEBUG :=

# Add the created iocs and sims to the install target
TARGETS += $(patsubst %, ../../makeIocs/O.Common/%, $(IOCS)) $(IOC_DIRS)

# include standard rules
include $(TOP)/configure/RULES

# General rule for building a Standard IOC from an XML file
../../makeIocs/O.Common/%: ../%.xml
	$(RMDIR) ../O.Common/tmp $@     
	$(XMLBUILDER) $(DEBUG) -e -o ../O.Common/tmp $<
	$(MV) ../O.Common/tmp/$(*F) $@

# General rule for building a Simulation IOC from an XML file
../../makeIocs/O.Common/%_sim: ../%.xml
	$(RMDIR) ../O.Common/tmp $@
	$(XMLBUILDER) $(DEBUG) --sim=linux-x86 -e -o ../O.Common/tmp $<
	$(MV) ../O.Common/tmp/$(*F)_sim $@

# Alternate rule for building a Standard IOC from a python file
../../makeIocs/O.Common/%: ../%.py
	$(RMDIR) ../O.Common/tmp $@
	./$< $(DEBUG) -e -o ../O.Common/tmp $*
	$(MV) ../O.Common/tmp/$(*F) $@

# Alternate rule for building a Simulation IOC from a python file
../../makeIocs/O.Common/%_sim: ../%.py
	$(RMDIR) ../O.Common/tmp $@
	./$< $(DEBUG) --sim=linux-x86 -e -o ../O.Common/tmp $*
	$(MV) ../O.Common/tmp/$(*F)_sim $@

# Install ioc from O.Common
$(TOP)/iocs/%: ../../makeIocs/O.Common/%
	$(RMDIR) $@
	$(CP) -rf $< $@

# Special rule to get rid of installed ioc
uninstall:
	$(RMDIR) $(IOC_DIRS)
