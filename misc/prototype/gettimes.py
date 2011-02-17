#!/usr/bin/env dls-python2.4

import sys, re
from numpy import *

filename = sys.argv[1]

ts = []
n = 0
for line in file(filename):
    m = re.match("tick @ (.*?) (.*)", line)
    if m:
        (sec, nsec) = m.groups()
        t = float(sec) + 1e-9 * float(nsec)
        ts.append(t - 1e-3 * n)
        n += 1

ts = array(ts[1:-1]) * 1e6
print "Samples %d" % len(ts)
ts0 = ts - mean(ts)
print "Standard deviation %f us" % std(ts0)
print "Max abs %f us" % max(abs(ts0))
print "argmax is %d" % argmax(abs(ts0))

from pkg_resources import require
require("matplotlib")
from pylab import *

overrun = len(nonzero(abs(ts0) > 100)[0])

hist(ts0, 100)
title("EtherCAT jitter, %d samples, %d > 100us" % (len(ts0), overrun))
xlabel("Time (us)")
ylabel("Count")
show()




        
