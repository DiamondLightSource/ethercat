#!/bin/env python2.6
#
# script to list ethercat device types available in one file

from pkg_resources import require
require("iocbuilder==3.24")
import iocbuilder
import os, sys

iocbuilder.ConfigureIOC(architecture = 'linux-x86')
iocbuilder.ModuleVersion('asyn','4-19')

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

def usage():
    print """examine_file.py: print device descriptions in file

Usage:
         %(scriptname)s [-a] file.xml [ file2.xml file3.xml ... ]

Names returned are filtered to reflect only devices supported at DLS
Options:
    -a  Shows all devices, does not filter for DLS supported devices

Example:
      %(scriptname)s ../xml/NI9144.xml 
""" % dict(scriptname=__file__)
    sys.exit(1)

doFilter = True

def keyRepr(key):
    ( typename, revision ) = key
    return "%s %d" % (typename, revision)

if __name__ == "__main__":
    if len(sys.argv) >= 2:
        if sys.argv[1] == "-a":
            sys.argv.pop(1)
            doFilter = False
        if sys.argv[1] == "-h":
            usage()
    else:
        usage()
    while len(sys.argv) > 1:
        dev_set = ethercat.getDescriptions(sys.argv[1])
        # filtered set
        fset = dev_set
        if doFilter:
            fset = ethercat.filteredDescriptions(dev_set)
        print """File: %(name)s
Number of entries: %(count)d (Filter: %(filtered)s)
""" % dict(name = sys.argv[1], count = len(fset), filtered=doFilter)
        for k in sorted(fset.keys(), key=keyRepr):
            print "%s rev 0x%08x" % k
        sys.argv.pop(1)

