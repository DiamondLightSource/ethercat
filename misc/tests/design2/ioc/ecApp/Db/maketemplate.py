#!/usr/bin/python2.4

# need system python for libxml2 support with xpath

import libxml2

## reqs = set([(0x00000002, 0x044d2c52, 0x00110000),
##             (0x00000002, 0x03ec3052, 0x00100000),
##             (0x00000002, 0x05de3052, 0x00100000)])

reqs = set()

def parsePdoEntry(pdoname, entry, os):
    # some pdo entries are just padding with no name, ignore
    try:
        name = entry.xpathEval("Name")[0].content
    except:
        return
    index = parseInt(entry.xpathEval("Index")[0].content)
    subindex = parseInt(entry.xpathEval("SubIndex")[0].content)
    bitlen = parseInt(entry.xpathEval("BitLen")[0].content)
    datatype = entry.xpathEval("DataType")[0].content
    
def parsePdo(pdo, d, os):
    # some pdos don't have a sync manager assigment
    # not supported by EtherLab driver
    if not pdo.xpathEval("@Sm"):
        return
    name = pdo.xpathEval("Name")[0].content
    name = name.replace(" ", "")
    index = parseInt(pdo.xpathEval("Index")[0].content)
    for entry in pdo.xpathEval("Entry"):
        parsePdoEntry(name, entry, index in os)
    return name
    
def parseInt(text):
    if text.startswith("#x") or text.startswith("0x"):
        return int(text.replace("#x", ""), 16)
    else:
        return int(text)

longin_text = """
record(longin, "$(DEVICE):%(name)s")
{
  field("DTYP", "asynInt32")
  field("INP",  "@asyn($(PORT))%(command)s")
  field("SCAN", "$(SCAN)")
}
"""

def makeTemplate(name, longin, longout):
    f = file("%s.template" % name, "w")
    for l in longin:
        print >> f, longin_text % {"name": l, "command": l}
    f.close()

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

            longin = []
            longout = []
            
            for dcmode in device.xpathEval("Dc/OpMode/Sm/Pdo[@OSFac]"):
                oversampling.add(parseInt(dcmode.content))
            for txpdo in device.xpathEval("TxPdo"):
                longin.append(parsePdo(txpdo, 1, oversampling))
            for rxpdo in device.xpathEval("RxPdo"):
                longout.append(parsePdo(rxpdo, 0, oversampling))

            makeTemplate(name, longin, longout)

if __name__ == "__main__":
    import sys
    chain = sys.argv[1]
    doc = libxml2.parseFile(chain)

    for d in doc.xpathEval("//device"):
        name = d.xpathEval("@type_name")[0].content
        revision = parseInt(d.xpathEval("@revision")[0].content)
        reqs.add((name, revision))
        # reqs.add(name)

    base = "/home/jr76/ethercat/xml"
    import os
    for f in os.listdir(base):
        if f.endswith("xml"):
            filename = os.path.join(base, f)
            parseFile(filename)



# loads chain description, outputs complete config file...
