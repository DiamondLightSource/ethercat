CFLAGS=-O2 -g -I/dls_sw/work/common/rt/ethercat/include -Wall
LDLIBS=-lpthread -lrt -lethercat -lm
LDFLAGS=-L/dls_sw/work/common/rt/ethercat/lib -Wl,--rpath=/dls_sw/work/common/rt/ethercat/lib
all: server client capture 

server: server.c config.o

capture: capture.c config.o

config.o: config.c config.h

clean:
	rm server client capture config.o capture3702

install: server 
	echo "Nothing to install"
