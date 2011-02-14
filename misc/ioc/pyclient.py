#!/usr/bin/env dls-python2.4

from pkg_resources import require
require("cothread")
from cothread.catools import *
from cothread import Sleep

from math import sin, pi

N = 1000.0
n = 0
freq = 10
while 1:
    v = (1 + sin(2.0 * pi * n / N * freq)) * 2000
    #print v
    Sleep(1.0 / N)
    caput("ECTESTOUT", v)
    n += 1



