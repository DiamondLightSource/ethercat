# build the descriptions pickle file for ethercat w/o iocbuilder
import build_iocbuilder_descriptions

# using class "ethercat"
# pickled descriptions in this file
ethercat_descriptions = "ethercat_descriptions.pkl"

def build_descriptions():
    import ethercat
    import os
    import pickle
    builder_dir = os.path.dirname(__file__)
    etc_dir = os.path.realpath(os.path.join(builder_dir,'..'))
    xml_dir = os.path.realpath(os.path.join(etc_dir,'xml'))
    fullpath=os.path.join(builder_dir,ethercat_descriptions)

    dev_descriptions = dict()
    for f in build_iocbuilder_descriptions.slaveInfoFiles:
        filename = os.path.join(xml_dir, f)
        for key, dev in ethercat.getDescriptions(filename).iteritems():
            typename = key[0]
            revision = key[1]
            dev_descriptions[key] = dev

    with open(fullpath,"w") as descriptionsfile:
        pickle.dump(dev_descriptions,descriptionsfile)

if __name__ == "__main__":
    build_descriptions()
