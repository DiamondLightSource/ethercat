ETHERCAT=/dls_sw/work/common/rt/ethercat
UTILS=$(CURDIR)/../../utils

CFLAGS=-I$(ETHERCAT)/include -O2 -g -I$(UTILS)/src -Wall -I/usr/include/libxml2
LDFLAGS=-L$(ETHERCAT)/lib -Wl,-rpath=$(ETHERCAT)/lib -Wl,-rpath=$(UTILS)/lib -L$(UTILS)/lib
LDLIBS=-lethercat -lrt -lpthread -lComrt -lm -lxml2

TARGETS=bus_scan2 qordertest

configparser: CFLAGS:=$(CFLAGS) -Dmain_TEST=main

all: $(TARGETS)

bus_monitor: bus_monitor.c configparser.c ethercat_device.c
bus_scan2: bus_scan2.c configparser.c ethercat_device.c timer.c
bus_scan_socket: bus_scan_socket.c configparser.c ethercat_device.c timer.c

bus_scan: bus_scan.c EL3162.c EK1101.c EL4102.c EL3702.c master_device.c
	$(CC) $(CFLAGS) $^ -o $@  $(LDFLAGS) $(LDLIBS) 


clean: 
	-rm $(TARGETS) *~


