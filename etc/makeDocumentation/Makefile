TOP = ../..
include $(TOP)/configure/CONFIG

# set to -d to get debugging
DEBUG :=

# this is the doxygen output dir
DOCDIR := $(TOP)/documentation/doxygen

# add the documentation files to the install target
install: $(DOCDIR)

# rule for creating the doxygen documentation
#$(DOCDIR): config.src manual.src $(DOCDIR)/build_instructions_test
$(DOCDIR): config.src manual.src classes.src \
	config.src devices.src generic_adc.src \
	getting_started.src license.src release_notes.src \
	sdo_support.src chain_description.src release_notes.src \
    slave_addressing.src
	mkdir -p $(DOCDIR)
	dls-make-doxygen-documentation.py -o $(DOCDIR) config.src manual.src
	cp configurable-error-suppression.patch $(DOCDIR)
	cp avoid-syslog-spam-when-bus-loses-power.patch $(DOCDIR)

# # rule for generating build instructions from an xml file
# $(DOCDIR)/build_instructions_%: $(TOP)/etc/makeIocs/%.xml
# 	mkdir -p $(DOCDIR)
# 	dls-xml-iocbuilder.py --doc=$@ $(DEBUG) $^

# # rule for generating build instructions from a py file
# $(DOCDIR)/build_instructions_%: $(TOP)/etc/makeIocs/%.py
# 	mkdir -p $(DOCDIR)
# 	$^ --doc=$@ $(DEBUG) example

# don't do anything in clean
clean: ;

# Special rule to get rid of installed documentation dir
uninstall:
	$(RMDIR) $(DOCDIR)
