ETHERCAT=/dls_sw/work/common/rt/ethercat
UTILS=$(PWD)/../utils

CFLAGS=-I$(ETHERCAT)/include -O2 -g -I$(UTILS)/src -Wall
LDFLAGS=-L$(ETHERCAT)/lib -Wl,-rpath=$(ETHERCAT)/lib -Wl,-rpath=$(UTILS)/lib -L$(UTILS)/lib
LDLIBS=-lethercat -lrt -lpthread -lComrt -lm

TARGETS=dc_user rt_test out_fail scantest bus_scan configtest

all: $(TARGETS)

bus_scan: bus_scan.c EL3162.c EK1101.c EL4102.c EL3702.c master_device.c
	$(CC) $(CFLAGS) $^ -o $@  $(LDFLAGS) $(LDLIBS) 


clean: 
	-rm $(TARGETS) *~


