
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

def main():
    import os
    import pickle
    import pkg_resources
    pkg_resources.require('iocbuilder==3.70')
    #import sys
    #sys.path.insert(0,"/home/rjq35657/common/python/iocbuilder")

    import iocbuilder
    iocbuilder.ConfigureIOC(architecture = 'linux-x86_64')
    from iocbuilder import ModuleVersion
    ModuleVersion('asyn', '4-34')
    ModuleVersion('busy', '1-7dls1')
    ModuleVersion('ethercat', home='/dls_sw/work/R3.14.12.7/support')

    from iocbuilder.modules import ethercat

    base = ethercat.ethercat.builder_dir
    dev_descriptions = dict()
    for f in slaveInfoFiles:
        filename = os.path.join(base, f)
        for key, dev in ethercat.ethercat.getDescriptions(filename).iteritems():
            typename = key[0]
            revision = key[1]
            dev_descriptions[key] = dev

    with open(cache,"w") as cachefile:
        pickle.dump(dev_descriptions,cachefile)
    
if __name__ == "__main__":
    main()
    



