#!/bin/env dls-python
import os, sys

# import ethercat.py from this module's builder
scripts_dir = os.path.dirname(os.path.abspath(__file__))
etc_dir = os.path.realpath(os.path.join(scripts_dir,'../'))
builder_dir = os.path.realpath(os.path.join(etc_dir, 'builder'))

if builder_dir not in sys.path:
    sys.path.insert(0, builder_dir)

import ethercat

def main():
    if len(sys.argv) != 2:
        print "usage: %s chain.xml" % (sys.argv[0])
        sys.exit(1)
    chainfile = sys.argv[1]

    import libxml2
    doc = libxml2.parseFile(chainfile)

    chain = ethercat.EthercatChain()
    chainPorts = {} # dictionary of chainElements by port name
    sdoDict = {} # sdos indexed by sdo name

    for d in doc.xpathEval("//device") + doc.xpathEval("//ethercat.EthercatSlave"):
        type_names = d.xpathEval("@type_name")
        if type_names:
            name = type_names[0].content
            revision = ethercat.parseInt(d.xpathEval("@revision")[0].content)
            type_rev = "%s rev 0x%08x" % (name, revision)
        else:
            type_rev = d.xpathEval("@type_rev")[0].content
        position = d.xpathEval("@position")[0].content
        if not position.startswith("DCS"):
            position = ethercat.parseInt(position)
        portname = d.xpathEval("@name")[0].content
        oversample = 0
        oversample_xml = d.xpathEval("@oversample")
        if len(oversample_xml):
           oversample = ethercat.parseInt(d.xpathEval("@oversample")[0].content)
        chainElem = ethercat.EthercatChainElem(type_rev, position, portname, oversample)
        chainPorts[portname] = chainElem
        chain.setDevice(chainElem)
    for d in doc.xpathEval("//ethercat.PdoAssignment"):
        slave = d.xpathEval("@slave")[0].content
        pdo_index = ethercat.parseInt(d.xpathEval("@pdo_index")[0].content)
        smnumber= ethercat.parseInt(d.xpathEval("@smnumber")[0].content)
        # name= d.xpathEval("@name")[0].content
        chainPorts[slave].assignPdo(smnumber, pdo_index)
    for d in doc.xpathEval("//ethercat.SdoControl"):
        name = d.xpathEval("@name")[0].content
        slave_name = d.xpathEval("@slave")[0].content
        index = ethercat.parseInt(d.xpathEval("@index")[0].content)
        sdo = ethercat.Sdo(name, slave_name, index)
        chainPorts[slave_name].assignSdo(sdo)
        sdoDict[name] = sdo
    for d in doc.xpathEval("//ethercat.SdoEntryControl"):
        asynparameter = d.xpathEval("@asynparameter")[0].content
        bit_length = ethercat.parseInt(d.xpathEval("@bit_length")[0].content)
        description = d.xpathEval("@description")[0].content
        name = d.xpathEval("@name")[0].content
        parentsdo = sdoDict[d.xpathEval("@parentsdo")[0].content]
        subindex = ethercat.parseInt(d.xpathEval("@subindex")[0].content)
        sdoentry = ethercat.SdoEntry(parentsdo, name,
                                     asynparameter, description,
                                     subindex, bit_length)
    ethercat.initialise()
    chain.getDeviceDescriptions()
    print chain.generateMasterXml()

if __name__ == "__main__":
    main()

# loads chain description, outputs complete config file...
