#!/bin/env python2.6
#
# script to list ethercat device types supported at 
# DLS, filtered according to the entries in 
# http://www.cs.diamond.ac.uk/cgi-bin/wiki.cgi/SupportedEtherCATModules

from pkg_resources import require
require("iocbuilder==3.24")
import iocbuilder
import os, sys

iocbuilder.ConfigureIOC(architecture = 'linux-x86')
iocbuilder.ModuleVersion('asyn','4-17')    
iocbuilder.ModuleVersion('ethercat',home=os.path.realpath('../../..'))

from iocbuilder.modules import ethercat

def usage():
    print """diamond_types.py: print names valid for ethercat slaves

Usage:
         %s [-a]

Names returned are filtered to reflect only devices supported at DLS
Options:
    -a  Shows all devices, does not filter for DLS supported devices""" % __file__
    sys.exit(1)

doFilter = True

def getDiamondDeviceSet():
    '''return list of devices filtered according to diamondFilter'''
    if doFilter:
        return ethercat.EthercatSlave._types_dict
    else:
        return ethercat.EthercatSlave._all_types_dict

def keyRepr(key):
    ( typename, revision ) = key
    return "%s %d" % (typename, revision)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        if sys.argv[1] == "-a":
            doFilter = False
        if sys.argv[1] == "-h":
            usage()
    dev_set = getDiamondDeviceSet()
    for k in sorted(dev_set.keys(), key=keyRepr):
        print "%s rev 0x%08x" % k
