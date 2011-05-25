TOP=..

include $(TOP)/configure/CONFIG

install:
	$(ECASYN)/etc/scripts/expandChain.py $(ECASYN)/etc/xml chain.xml > scanner.xml

clean:
	-rm scanner.xml

uninstall:


