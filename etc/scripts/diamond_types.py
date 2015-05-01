#!/bin/env dls-python
#
# script to list ethercat device types supported at 
# DLS, filtered according to the entries in 
# http://www.cs.diamond.ac.uk/cgi-bin/wiki.cgi/SupportedEtherCATModules

import os, sys

# import $(TOP)/etc/builder/ethercat.py from this module
scripts_dir = os.path.dirname(os.path.abspath(__file__))
top_dir = os.path.realpath(os.path.join(scripts_dir,'../..'))
builder_dir = os.path.realpath(os.path.join(top_dir, 'etc','builder'))

if builder_dir not in sys.path:
    sys.path.insert(0, builder_dir)

import ethercat

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
        return ethercat.filteredDescriptions(ethercat.getAllDevices())
    else:
        return ethercat.getAllDevices()

def keyRepr(key):
    ( typename, revision ) = key
    return "%s %d" % (typename, revision)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        if sys.argv[1] == "-a":
            doFilter = False
        if sys.argv[1] == "-h":
            usage()
    ethercat.initialise()
    dev_set = getDiamondDeviceSet()
    for k in sorted(dev_set.keys(), key=keyRepr):
        print "%s rev 0x%08x" % k
