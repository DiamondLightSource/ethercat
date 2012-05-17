#!/usr/bin/env python

"minimal ethercat master using an EL3314 at position 1"
"UDP version - not supported by any real devices!"

# These are the sync manager settings from the master (probably comes from the EEPROM)
# SM0: PhysAddr 0x1000, DefaultSize  128, ControlRegister 0x26, Enable 1
# SM1: PhysAddr 0x1080, DefaultSize  128, ControlRegister 0x22, Enable 1
# SM2: PhysAddr 0x1100, DefaultSize    0, ControlRegister 0x24, Enable 0
# SM3: PhysAddr 0x1180, DefaultSize   16, ControlRegister 0x20, Enable 1
#   TxPDO 0x1a00 "TC TxPDO-MapCh.1"
#   PDO entry 0x6000:01,  1 bit, "Underrange"
#   PDO entry 0x6000:02,  1 bit, "Overrange"
#   PDO entry 0x6000:03,  2 bit, "Limit 1"
#   PDO entry 0x6000:05,  2 bit, "Limit 2"
#   PDO entry 0x6000:07,  1 bit, "Error"
#   PDO entry 0x0000:00,  7 bit, "Gap"
#   PDO entry 0x6000:0f,  1 bit, "TxPDO State"
#   PDO entry 0x1800:09,  1 bit, ""
#   PDO entry 0x6000:11, 16 bit, "Value"

import socket, struct, time

# ethercat command types
APRD = 1
APWR = 2
BRD  = 7

# my MAC address (!)
MAC = [0x00, 0x1b, 0x21, 0xb1, 0xde, 0xfc]

def ethercmd(mac, cmd, index, slave, offset, data = "\x00\x00"):
    wc = 0
    interrupt = 0
    datalength = len(data)
    fmt = "<BBBBhHHH%dsH" % datalength
    sz = struct.calcsize(fmt) - 16
    pkt = struct.pack(fmt,
                      sz, 0x10,
                      cmd, index, slave, offset,
                      datalength, interrupt, data, wc)
    return pkt
    if len(pkt) < 60:
        pkt = pkt + ("\x00" * (60 - len(pkt)))
    return pkt

syncio = struct.pack("<HHBBBBHHBBBB",
                     0x1100, 0x0000, 0x24, 0x00, 0x00, 0x00,
                     0x1180, 0x0010, 0x20, 0x00, 0x01, 0x00)
                   
syncmb = struct.pack("<HHBBBBHHBBBB",
                     0x1000, 0x0080, 0x26, 0x00, 0x01, 0x00,
                     0x1080, 0x0080, 0x22, 0x00, 0x01, 0x00)

def sndrcv(sock, data):
    sock.sendto(data, ("192.168.0.255", 0x88a4))
    try:
        val = sock.recv(64)
        # print repr(val)
    except:
        print "timeout"
    return val
        
def init():
    # INIT STATE
    sndrcv(soc,ethercmd(MAC, APWR, 1, -1, 0x0120, data = "\x01\x00"))
    time.sleep(0.1)
    # READ AL STATUS
    sndrcv(soc,ethercmd(MAC, APRD, 1, -1, 0x0130))
    time.sleep(0.1)
    # SETUP MAILBOX SYNC MANAGER
    sndrcv(soc,ethercmd(MAC, APWR, 1, -1, 0x0800, data = syncmb))
    time.sleep(0.1)
    # SETUP PDO SYNC MANAGER
    sndrcv(soc,ethercmd(MAC, APWR, 1, -1, 0x0810, data = syncio))
    time.sleep(0.1)
    # READ AL STATUS
    sndrcv(soc,ethercmd(MAC, APRD, 1, -1, 0x0130))
    time.sleep(0.1)
    # READ AL STATUS CODE
    sndrcv(soc,ethercmd(MAC, APRD, 1, -1, 0x0134))
    time.sleep(0.1)
    # PRE-OP STATE
    sndrcv(soc,ethercmd(MAC, APWR, 1, -1, 0x0120, data = "\x02\x00"))
    time.sleep(0.1)
    # READ AL STATUS
    sndrcv(soc,ethercmd(MAC, APRD, 1, -1, 0x0130))
    time.sleep(0.1)
    # READ AL STATUS CODE
    sndrcv(soc,ethercmd(MAC, APRD, 1, -1, 0x0134))
    time.sleep(0.1)
    # SAFEOP STATE
    sndrcv(soc,ethercmd(MAC, APWR, 1, -1, 0x0120, data = "\x04\x00"))
    time.sleep(0.1)
    # READ AL STATUS
    sndrcv(soc,ethercmd(MAC, APRD, 1, -1, 0x0130))
    time.sleep(0.1)
    # READ AL STATUS CODE
    sndrcv(soc,ethercmd(MAC, APRD, 1, -1, 0x0134))
    time.sleep(0.1)

    # OP STATE (enables outputs)
    # sndrcv(soc,ethercmd(MAC, APWR, 1, -1, 0x0120, data = "\x08\x00"))
    # time.sleep(0.1)

    sndrcv(soc,ethercmd(MAC, APRD, 1, -1, 0x0130))
    time.sleep(0.1)
    sndrcv(soc,ethercmd(MAC, APRD, 1, -1, 0x0134))
    time.sleep(0.1)

    sndrcv(soc,ethercmd(MAC, APRD, 1, -1, 0x1180, data = "\x00" * 16))

soc = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
soc.settimeout(0.5)
soc.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
# soc.bind(("p4p1", 0x88a4))
soc.bind(("192.168.0.1", 0x88a4))
# soc.connect(("<broadcast>", 0x88a4))

init()

while 1:
    temp = sndrcv(soc,ethercmd(MAC, APRD, 1, -1, 0x1180, data = "\x00" * 16))
    print "temperature", struct.unpack("<H", temp[28:30])[0] / 10.0
    time.sleep(0.1)
    
