TOP=..

include $(TOP)/configure/CONFIG

install:
	$(ETHERCAT)/etc/scripts/expandChain.py $(ETHERCAT)/etc/xml simulation-test-chain.xml > simulationTest.xml
	echo $(ETHERCAT)/bin/linux-x86/scanner -s $(PWD)/etc/simulationTest.xml /tmp/scanner > scanner.sh
	chmod +x scanner.sh

clean:
	-rm simulationTest.xml

uninstall:


