

# build offline a set of device descrptions for getAllDevices

base = "/dls_sw/work/R3.14.12.7/support/ethercat/etc/xml"

import ethercat
import os
import pickle

cache = "cache.pkl"
slaveInfoFiles = [
    "Beckhoff EL2xxx.xml",
    "Beckhoff EL1xxx.xml",
    "Beckhoff EL31xx.xml",
    "Beckhoff EK11xx.xml",
    "Beckhoff EL15xx.xml",
    "Beckhoff EL32xx.xml",
    "Beckhoff EL33xx.xml",
    "Beckhoff EL3xxx.xml",
    "Beckhoff EL37xx.xml",
    "Beckhoff EL4xxx.xml",
    "Beckhoff EL47xx.xml",
    "Beckhoff EL9xxx.xml",
    "Beckhoff EP1xxx.xml",
    "Beckhoff EP2xxx.xml",
    "Beckhoff EP3xxx.xml",
    "Beckhoff EP4xxx.xml",
    "SMC EX250-SEN1-X156.xml",
    "SMC EX260-SECx_V11.xml",
    "NI9144.xml"
    ]

if __name__ == "__main__":
    dev_descriptions = dict()
    for f in slaveInfoFiles:
        filename = os.path.join(base, f)
        for key, dev in ethercat.getDescriptions(filename).iteritems():
            typename = key[0]
            revision = key[1]
            dev_descriptions[key] = dev

    #print(dev_descriptions)
    with open(cache,"w") as cachefile:
        pickle.dump(dev_descriptions,cachefile)


