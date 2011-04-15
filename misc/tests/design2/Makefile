ETHERCAT=/dls_sw/work/common/rt/ethercat
UTILS=$(CURDIR)/../../utils

CFLAGS=-I$(ETHERCAT)/include -O2 -g -I$(UTILS)/src -Wall -I/usr/include/libxml2
LDFLAGS=-L$(ETHERCAT)/lib -Wl,-rpath=$(ETHERCAT)/lib -Wl,-rpath=$(UTILS)/lib -L$(UTILS)/lib -L.
LDLIBS=-lethercat -lrt -lpthread -lComrt -lm -lxml2 -lScanner

TARGETS=bus_scan2 qordertest scanner.xml libScanner.a

all: $(TARGETS)

scanner.xml: chain.xml
	./beck.py > $@

libScanner.a: parser.o classes.o
	ar rcs $@ $^

# parser: CFLAGS:=$(CFLAGS) -Dmain_TEST_PARSER=main classes.c

bus_scan2: libScanner.a bus_scan2.c unpack.c

clean: 
	-rm $(TARGETS) *~


