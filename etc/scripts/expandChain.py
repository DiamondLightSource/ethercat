#!/bin/env python

from pkg_resources import require
require("iocbuilder==3.24")
import iocbuilder
import os, sys

scripts_dir = os.path.realpath(os.path.dirname(__file__))
assert scripts_dir.endswith("/ethercat/etc/scripts"), \
        "Unexpected module name - should be 'ethercat'"
module_home = os.path.join(scripts_dir, '../../..')
iocbuilder.ConfigureIOC(architecture = 'linux-x86')
iocbuilder.ModuleVersion('asyn','4-17')
iocbuilder.ModuleVersion('ethercat',home=os.path.realpath(module_home))
from iocbuilder.modules import ethercat
from iocbuilder.modules.ethercat import parseInt, EthercatSlave, EthercatMaster

def main():
    if len(sys.argv) != 2:
        print "usage: %s chain.xml" % (sys.argv[0])
        sys.exit(1)
    chainfile = sys.argv[1]

    import libxml2
    doc = libxml2.parseFile(chainfile)

    master = EthercatMaster('/tmp/dummy')
    for d in doc.xpathEval("//device"):
        name = d.xpathEval("@type_name")[0].content
        revision = parseInt(d.xpathEval("@revision")[0].content)
        position = parseInt(d.xpathEval("@position")[0].content) 
        portname = d.xpathEval("@name")[0].content
        type_rev = "%s rev 0x%08x" % (name, revision)
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
