# build the cache for ethercat w/o iocbuilder
import offline

def build_cache():
    import ethercat
    import os
    import pickle
    builder_dir = os.path.dirname(__file__)
    etc_dir = os.path.realpath(os.path.join(builder_dir,'..'))
    xml_dir = os.path.realpath(os.path.join(etc_dir,'xml'))
    fullpath=os.path.join(builder_dir,offline.cache)

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
    build_cache()
