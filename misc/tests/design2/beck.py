#!/usr/bin/env python

import libxml2

reqs = set([(0x00000002, 0x044d2c52, 0x00110000),
            (0x00000002, 0x03ec3052, 0x00100000),
            (0x00000002, 0x05de3052, 0x00100000)])

def parsePdoEntry(entry):
    # some pdo entries are just padding with no name, ignore
    try:
        name = entry.xpathEval("Name")[0].content
    except:
        return
    index = parseInt(entry.xpathEval("Index")[0].content)
    subindex = parseInt(entry.xpathEval("SubIndex")[0].content)
    bitlen = parseInt(entry.xpathEval("BitLen")[0].content)
    print '      <entry name="%s" index="0x%08x" subindex="0x%08x" bit_length="%d" />' % (name, index, subindex, bitlen)

def parsePdo(pdo, d):
    # some pdos don't have a sync manager assigment
    # not supported by EtherLab driver
    if not pdo.xpathEval("@Sm"):
        return
    name = pdo.xpathEval("Name")[0].content
    index = parseInt(pdo.xpathEval("Index")[0].content)
    print '    <sync index="0" dir="0" watchdog="0">'
    print '    <pdo dir="%d" name="%s" index="0x%08x">' % (d, name, index)
    for entry in pdo.xpathEval("Entry"):
        parsePdoEntry(entry)
    print '    </pdo>'
    print '    </sync>'
    
def parseInt(text):
    if text.startswith("#x"):
        return int(text.replace("#x", ""), 16)
    else:
        return int(text)

def parseFile(filename):
    doc = libxml2.parseFile(filename)
    vendor = parseInt(doc.xpathEval("//Vendor/Id")[0].content)
    for device in doc.xpathEval("//Device"):
        try:
            product = parseInt(device.xpathEval("Type/@ProductCode")[0].content)
        except:
            continue
        revision = parseInt(device.xpathEval("Type/@RevisionNo")[0].content)
        key = (vendor, product, revision)
        if key in reqs:
            name = device.xpathEval("Type")[0].content
            print '  <device name="%s" vendor="0x%08x" product="0x%08x" revision="0x%08x">' % (name, vendor, product, revision)
            for txpdo in device.xpathEval("TxPdo"):
                parsePdo(txpdo, 1)
            for rxpdo in device.xpathEval("RxPdo"):
                parsePdo(rxpdo, 0)
            print '  </device>'

if __name__ == "__main__":
    # filename = "/home/jr76/ethercat/xml/Beckhoff EKxxxx.xml"
    base = "/home/jr76/ethercat/xml"
    import os
    print "<devices>"
    for f in os.listdir(base):
        if f.endswith("xml"):
            filename = os.path.join(base, f)
            # print filename
            parseFile(filename)
    print "</devices>"
