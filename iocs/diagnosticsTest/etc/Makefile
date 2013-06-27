TOP=..

include $(TOP)/configure/CONFIG

install:
	$(ETHERCAT)/etc/scripts/expandChain.py diagnostics-test-chain.xml > diagnosticsTest.xml
	echo $(ETHERCAT)/bin/linux-x86/scanner -s $(PWD)/etc/diagnosticsTest.xml /tmp/scanner > scanner.sh
	chmod +x scanner.sh

clean:
	-rm diagnosticsTest.xml

uninstall:


