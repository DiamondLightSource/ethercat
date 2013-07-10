TOP=..

include $(TOP)/configure/CONFIG

install:
	$(ETHERCAT)/etc/scripts/expandChain.py diagnostics-test-chain.xml > diagnosticsTest.xml
	echo "#!/bin/sh" > scanner.sh
	echo "cd \"$(dirname \"$0\")\"" >> scanner.sh
	echo "exec $(ETHERCAT)/bin/linux-x86_64/scanner ./diagnosticsTest.xml /tmp/scanner" >> scanner.sh
	chmod +x scanner.sh

clean:
	-rm diagnosticsTest.xml
	-rm scanner.sh

uninstall:


