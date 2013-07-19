#!/bin/env dls-python

import os, sys
from pkg_resources import require
require("iocbuilder==3.38")
import iocbuilder
import iocbuilder.libversion
from iocbuilder.libversion import ModuleVersion

# import $(TOP)/etc/builder/ethercat.py from this module
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

    master = EthercatMaster('/tmp/dummy')
    for d in doc.xpathEval("//device") + doc.xpathEval("//ethercat.EthercatSlave"):
        type_names = d.xpathEval("@type_name")
        if type_names:
            name = type_names[0].content
            revision = parseInt(d.xpathEval("@revision")[0].content)
            type_rev = "%s rev 0x%08x" % (name, revision)
        else:
            type_rev = d.xpathEval("@type_rev")[0].content
        position = d.xpathEval("@position")[0].content
        if not position.startswith("DCS"):
            position = parseInt(position)
        portname = d.xpathEval("@name")[0].content
        oversample = 0
        oversample_xml = d.xpathEval("@oversample")
        if len(oversample_xml):
           oversample = parseInt(d.xpathEval("@oversample")[0].content)
        EthercatSlave(master, position, portname, type_rev, oversample)
    master.getDeviceDescriptions()
    print master.generateMasterXml()

if __name__ == "__main__":
    main()


# loads chain description, outputs complete config file...
