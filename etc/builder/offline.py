
#cache files
# using class "iocbuilder.module.ethercat"
# this is generated in this file offline.py
cache = "cache.pkl"

# this is generated in offline1.py
# using class "ethercat"
cache1 = "cache1.pkl"

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

    import iocbuilder
    iocbuilder.ConfigureIOC(architecture = 'linux-x86_64')
    from iocbuilder import ModuleVersion
    builder_dir = os.path.dirname(__file__)
    etc_dir = os.path.realpath(os.path.join(builder_dir,'..'))
    home_dir = os.path.realpath(os.path.join(
        etc_dir,'..','..'))
    xml_dir = os.path.realpath(os.path.join(
        etc_dir,'xml'))
    fullpath=os.path.join(builder_dir,cache)

    ModuleVersion('asyn', '4-34')
    ModuleVersion('busy', '1-7dls1')
    ModuleVersion('ethercat', home=home_dir)

    from iocbuilder.modules import ethercat

    dev_descriptions = dict()
    for f in slaveInfoFiles:
        filename = os.path.join(xml_dir, f)
        for key, dev in ethercat.ethercat.getDescriptions(filename).iteritems():
            typename = key[0]
            revision = key[1]
            dev_descriptions[key] = dev

    with open(fullpath,"w") as cachefile:
        pickle.dump(dev_descriptions,cachefile)
    
if __name__ == "__main__":
    main()
    



