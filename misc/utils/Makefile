CFLAGS=-Wall -g
LDLIBS=-lpthread -lrt -lm -lComrt -Wl,-rpath=$(PWD)/lib -L$(PWD)/lib
EPICS=/home/jr76/base/realtime/base-3-14-11

TARGETS=lib/libComrt.a test_server test_interval

all: $(TARGETS)

# testq clienttest rttest rtepics rtother rtemu

src/%.o: src/%.c
	gcc -c $^ -o $@ $(CFLAGS)

lib/libComrt.a: src/rtutils.o src/msgsock.o
	ar rcs $@ $^

% : src/%.c
	gcc $^ -o $@ $(CFLAGS) $(LDLIBS)

# rtepics: src/rtepics.c
# 	gcc $^ -o $@ $(CFLAGS) -I$(EPICS)/include -I$(EPICS)/include/os/Linux -L$(EPICS)/lib/linux-x86 -lCom -Wl,-rpath=$(EPICS)/lib/linux-x86

clean:
	rm $(TARGETS)


