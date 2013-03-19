#!/dls_sw/prod/tools/RHEL5/bin/dls-python2.6

from pkg_resources import require
require("iocbuilder==3.24")
import iocbuilder
import os, sys

iocbuilder.ConfigureIOC(architecture = 'linux-x86')
iocbuilder.ModuleVersion('asyn','4-17')

# work-around to import $(TOP)/etc/builder.py whether in prod or work
scripts_dir = os.path.realpath(os.path.dirname(__file__))
top_dir = os.path.realpath(os.path.join(scripts_dir, '../..'))
module_top, release_num= os.path.split(top_dir)
if module_top.endswith("/ethercat"):
    #released version of ethercat module
    iocbuilder.ModuleVersion('ethercat',release_num)
else:
    modules_home, module_name = os.path.split(top_dir)
    assert module_name == "ethercat", \
            "Unexpected module name - should be 'ethercat'"
    iocbuilder.ModuleVersion(module_name, home=modules_home)

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
