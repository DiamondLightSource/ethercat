#!/usr/bin/env dls-python2.6

import sys

from pkg_resources import require
require("matplotlib")
from pylab import *

def main(filename):

    data = []
    for line in file(filename):
        line = line.strip().split()
        data.append([float(line[0]), float(line[3])])
##     data = []
##     info = []
    
##     state = ""
##     for line in file(filename):
##         line = line.strip()
        
##         if state == "data":
##             if line.startswith("#"):
##                 state = "footer"
##             else:
##                 point = [float(x) for x in line.split()]
##                 data.append(point)

##         if state == "footer":
##             info.append(line[2:])
                
##         if line.startswith("# Histogram"):
##             state = "data"

##     info = " ".join(info)
    # data = zip(*[d for d in data if d[1] != 0])
    data = zip(*data)
    data[1] = log(data[1]) + 1e-9
    print data
    bar(data[0], data[1])
    xlabel('latency (us)')
    ylabel('LOG10(count)')
    ylim([ -1, max(data[1]) + 1])
    title("Dual-core RT-PREEMPT Latency")
    draw()
    show()
    
            
        
            
            
if __name__ == "__main__":
    filename = sys.argv[1]
    main(filename)


