Prototype Design
================

EtherCat scanner runs in own process serving a UNIX domain socket:

socket -> read thread  -> scanner input queue        -> scanner thread -> private work queue
          timer thread -> scanner input queue (prio) -> scanner thread
socket <- write thread <- client(n) result queue     <- scanner thread

Timer thread wakes up scanner thread at bus cycle rate:
Scanner thread takes commands from input queue and stores in private work queue.
On timer tick, scanner thread plays commands from local work queue (read/write).
Monitor commands are automatically re-queued. Reads and writes are one-shot.
Command results are sent to the appropriate client queue, 
the destination is stored in the commmand.

In-process:

Could replace the socket and make the scanner in-process using EPICS as the IPC.
Client queues are now per-record.

ca-client
=========
channel access client to write a sine wave to an EPICS PV 
Operation: start server program from "prototype" directory
           start asyn interface IOC "./test.sh" from ioc directory
           start ca-client "bin/linux-x86/caClient <pvname>" from ca-client directory


