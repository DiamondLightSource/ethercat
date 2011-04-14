ETHERCAT=/dls_sw/work/common/rt/ethercat
UTILS=$(CURDIR)/../../utils

CFLAGS=-I$(ETHERCAT)/include -O2 -g -I$(UTILS)/src -Wall -I/usr/include/libxml2
LDFLAGS=-L$(ETHERCAT)/lib -Wl,-rpath=$(ETHERCAT)/lib -Wl,-rpath=$(UTILS)/lib -L$(UTILS)/lib
LDLIBS=-lethercat -lrt -lpthread -lComrt -lm -lxml2

TARGETS=bus_scan2 qordertest parser scanner.xml

all: $(TARGETS)

scanner.xml: chain.xml
	./beck.py > $@

configparser: CFLAGS:=$(CFLAGS) -Dmain_TEST=main

parser: CFLAGS:=$(CFLAGS) -Dmain_TEST_PARSER=main utils.c classes.c

# bus_monitor: bus_monitor.c configparser.c ethercat_device.c
bus_scan2: bus_scan2.c parser.c utils.c classes.c unpack.c


clean: 
	-rm $(TARGETS) *~


