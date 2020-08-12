# build the second type of cache that uses ethercat w/o iocbuilder
import offline

def main():
    import ethercat
    import os
    import pickle
    builder_dir = os.path.dirname(__file__)
    etc_dir = os.path.realpath(os.path.join(builder_dir,'..'))
    xml_dir = os.path.realpath(os.path.join(etc_dir,'xml'))
    fullpath=os.path.join(builder_dir,offline.cache1)

    dev_descriptions = dict()
    for f in offline.slaveInfoFiles:
        filename = os.path.join(xml_dir, f)
        for key, dev in ethercat.getDescriptions(filename).iteritems():
            typename = key[0]
            revision = key[1]
            dev_descriptions[key] = dev

    with open(fullpath,"w") as cachefile:
        pickle.dump(dev_descriptions,cachefile)

if __name__ == "__main__":
    main()


