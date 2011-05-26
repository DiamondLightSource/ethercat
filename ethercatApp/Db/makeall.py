#!/usr/bin/env python

import os

for line in file("devices.txt"):
    (name, rev) = line.split()
    print name, rev
    os.system("../../etc/scripts/maketemplate.py -b ../../etc/xml -d %s -r %s -o %s.template" % (name, rev, name))
