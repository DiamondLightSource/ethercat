TOP=..

include $(TOP)/configure/CONFIG

install:
	$(ECASYN)/etc/scripts/expandChain.py chain.xml $(ECASYN)/etc/xml > scanner.xml

clean:
	-rm scanner.xml

uninstall:


