#!/usr/bin/python2.4

# need system python for libxml2 support with xpath

import libxml2

## reqs = set([(0x00000002, 0x044d2c52, 0x00110000),
##             (0x00000002, 0x03ec3052, 0x00100000),
##             (0x00000002, 0x05de3052, 0x00100000)])

reqs = set()

def parsePdoEntry(entry, os):
    # some pdo entries are just padding with no name, ignore
    try:
        name = entry.xpathEval("Name")[0].content
    except:
        return
    index = parseInt(entry.xpathEval("Index")[0].content)
    subindex = parseInt(entry.xpathEval("SubIndex")[0].content)
    bitlen = parseInt(entry.xpathEval("BitLen")[0].content)
    datatype = entry.xpathEval("DataType")[0].content
    print '      <entry name="%s" index="0x%08x" subindex="0x%08x" bit_length="%d" datatype="%s" oversample="%d" />' % (name, index, subindex, bitlen, datatype, os)

def parsePdo(pdo, d, os):
    # some pdos don't have a sync manager assigment
    # not supported by EtherLab driver
    if not pdo.xpathEval("@Sm"):
        return
    name = pdo.xpathEval("Name")[0].content
    index = parseInt(pdo.xpathEval("Index")[0].content)
    print '    <sync index="0" dir="0" watchdog="0">'
    print '    <pdo dir="%d" name="%s" index="0x%08x">' % (d, name, index)
    for entry in pdo.xpathEval("Entry"):
        parsePdoEntry(entry, index in os)
    print '    </pdo>'
    print '    </sync>'
    
def parseInt(text):
    if text.startswith("#x") or text.startswith("0x"):
        return int(text.replace("#x", ""), 16)
    else:
        return int(text)

def parseFile(filename):
    doc = libxml2.parseFile(filename)
    vendor = parseInt(doc.xpathEval("//Vendor/Id")[0].content)
    for device in doc.xpathEval("//Device"):
        try:
            name = device.xpathEval("Type")[0].content
            product = parseInt(device.xpathEval("Type/@ProductCode")[0].content)
            revision = parseInt(device.xpathEval("Type/@RevisionNo")[0].content)
        except:
            continue
        # key = (vendor, product, revision)
        key = (name, revision)

        oversampling = set()
        
        if key in reqs:
            aa = device.xpathEval("Dc/OpMode/AssignActivate")
            if aa:
                aa = parseInt(aa[0].content)
                print '  <device name="%s" vendor="0x%08x" product="0x%08x" revision="0x%08x" dcactivate="0x%08x">' % (name, vendor, product, revision, aa)
            else:
                print '  <device name="%s" vendor="0x%08x" product="0x%08x" revision="0x%08x">' % (name, vendor, product, revision)
            for dcmode in device.xpathEval("Dc/OpMode/Sm/Pdo[@OSFac]"):
                oversampling.add(parseInt(dcmode.content))
            for txpdo in device.xpathEval("TxPdo"):
                parsePdo(txpdo, 1, oversampling)
            for rxpdo in device.xpathEval("RxPdo"):
                parsePdo(rxpdo, 0, oversampling)
            print '  </device>'

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 3:
        print "usage: %s base-dir chain.xml" % (sys.argv[0])
        sys.exit(1)
    base = sys.argv[1]
    chain = sys.argv[2]

    doc = libxml2.parseFile(chain)

    for d in doc.xpathEval("//device"):
        name = d.xpathEval("@type_name")[0].content
        revision = parseInt(d.xpathEval("@revision")[0].content)
        reqs.add((name, revision))
        # reqs.add(name)

    import os
    print "<scanner>"
    print "<devices>"
    for f in os.listdir(base):
        if f.endswith("xml"):
            filename = os.path.join(base, f)
            parseFile(filename)

    print "</devices>"
    print doc.xpathEval("/chain")[0]
    print "</scanner>"

# loads chain description, outputs complete config file...
