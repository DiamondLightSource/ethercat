#!/bin/env dls-python

# script to check whether the revisions of a supported device[1]
# have modified names, by checking the xml description files
#
# http://www.cs.diamond.ac.uk/cgi-bin/wiki.cgi/SupportedEtherCATModules
#

from parseutil import getPdoName, getEntryName, parseInt, hasEntryName
import libxml2
import collections

verbose = True
# parseOutput = {
#     "devname" : [rev1, rev2, rev3]
#     }
# rev1 = {
#     "revision": rev1,
#     "inputs": [pdo1,pdo2],
#     "outputs": [pdos]
# }
# pdo1 = (datatype, pdo names, sm, index, subindex)

###### generate report
def compareNames(pdolist1, pdolist2):
    n1 = []
    for t in pdolist1:
        n1.append(t[1])
    # print n1
    n2 = []
    for t in pdolist2:
        n2.append(t[1])
    # print n2
    s1 = sorted(n1)
    s2 = sorted(n2)
    return s1 == s2

def compareTwoRevisions(r1,r2):
    "returns True if two revisions are the same"
    result = True
    #print "##"
    # print "r1 %s" % r1
    # print "r2 %s" % r2
    if len(r1["inputs"]) != len(r2["inputs"]):
        print "number of inputs differ between rev 0x%x and 0x%x" % \
            (r1["revision"], r2["revision"])
        result = False
    if len(r1["outputs"]) != len(r2["outputs"]):
        print "number of outputs differ between rev 0x%x and 0x%x" % \
            (r1["revision"], r2["revision"])
        result = False
    inputsnamescheck = compareNames(r1["inputs"],r2["inputs"])
    outputsnamescheck = compareNames(r1["outputs"],r2["outputs"])
    if not inputsnamescheck:
        print "input names differ between rev 0x%x and 0x%x" % \
            (r1["revision"], r2["revision"])
        result = False
    if not outputsnamescheck:
        print "outputs names differ between rev 0x%x and 0x%x" % \
            (r1["revision"], r2["revision"])
        result = False
    # if result:
    #     print "rev 0x%x and 0x%x did not change" % (r1["revision"], r2["revision"])
    return result

def compareAllRevisions(revlist):
    "returns True if all revisions are the same"
    if len(revlist) <= 1:
        return True
    result = True
    for index in range(len(revlist)-1):
        #print index
        r1 = compareTwoRevisions(revlist[index], revlist[index+1])
        result = result and r1
    return result

def report(data):
    print "Slave list %s" % data.keys()
    for n in data:
        print "device name: %s, revision count %d" % ( n, len(data[n]))
        compareAllRevisions(data[n])

###### read data

def parseFile(filename, devlist):
    """check xml file "filename" and return input and output pdos
       as a dictionary"""
    print "parsing file %s for devices %s" % (filename, devlist)
    doc = libxml2.parseFile(filename)
    vendor = parseInt(doc.xpathEval("//Vendor/Id")[0].content)
    devicedata = {} # dictionary index by device name
    for devtype in devlist:
        # print "devtype = %s" % devtype
        # collect the device nodes that match the device type
        xpath = "//Device/Type[.='%s']/.." % devtype

        for deviceNode in doc.xpathEval(xpath):
            devtypecheck = deviceNode.xpathEval("Type")[0].content
            assert(devtypecheck == devtype)
            product = parseInt(deviceNode.xpathEval("Type/@ProductCode")[0].content)
            revision = parseInt(deviceNode.xpathEval("Type/@RevisionNo")[0].content)
            dev_revision = parseDeviceRev(deviceNode, devtype, product, revision)
            if not devtype in devicedata:
                devicedata[devtype] = []
            devicedata[devtype].append(dev_revision)
    return devicedata

def parseDeviceRev(deviceNode, devtype,product, revision):
    devicerevdata = { "type": devtype,
                      "product": product,
                      "revision": revision,
                      "inputs" : [],
                      "outputs": []
    }
    for txpdo in deviceNode.xpathEval("TxPdo"):
        # pdos without sync manager entries are not default
        if txpdo.xpathEval("@Sm"):
            for entry in txpdo.xpathEval("Entry"):
                # some pdo entries are just padding with no name, ignore
                if hasEntryName(entry):
                    datatype = entry.xpathEval("DataType")[0].content
                    index = entry.xpathEval("Index")[0].content
                    subindex = entry.xpathEval("SubIndex")[0].content
                    name = getPdoName(txpdo) + "." + getEntryName(entry)
                    # print name
                    t = tuple([datatype, name, index, subindex])
                    devicerevdata["inputs"].append(t)
                elif verbose:
                    print "Ignoring TxPdo entry in pdo %s" % getPdoName(txpdo)
    for rxpdo in deviceNode.xpathEval("RxPdo"):
        # pdos without sync manager entries are not default
        if rxpdo.xpathEval("@Sm"):
            for entry in rxpdo.xpathEval("Entry"):
                # some pdo entries are just padding with no name, ignore
                if hasEntryName(entry):
                    datatype = entry.xpathEval("DataType")[0].content
                    index = entry.xpathEval("Index")[0].content
                    subindex = entry.xpathEval("SubIndex")[0].content
                    name = getPdoName(rxpdo) + "." + getEntryName(entry)
                    # print name
                    t = tuple( [datatype, name, index, subindex])
                    devicerevdata["outputs"].append(t)
                elif verbose:
                    print "Ignoring RxPdo entry in pdo %s" % getPdoName(txpdo)
    return devicerevdata

def main():
    prefix = "/scratch/rjq35657/exp/Beckhoff "
    # files = {"filename": [ "dev1", "dev2"], ... }
    files = collections.OrderedDict()
    files["EL1xxx.xml"]= ["EL1014", "EL1084"]
    files["EL15xx.xml"]= ["EL1502"]
    files["EL25xx.xml"]= ["EL2502", "EL2595"]
    files["EL32xx.xml"]= ["EL3202", "EL3202-0010"]
    files["EL33xx.xml"]= ["EL3314"]
    files["EL3xxx.xml"]= ["EL3602", "EL3202-0010", "ELM3004"]
    files["EL47xx.xml"]= ["EL4732"]
    files["EP2xxx.xml"]= ["EP2338-0001", "EP2338-0002","EP2624", "EP2624-0002"]
    files["EP3xxx.xml"]= ["EP3174-0002","EP3204-0002","EP3314-0002"]
    files["EP4xxx.xml"]= ["EP4174-0002","EP4374-0002"]

    for f in files:
        data = parseFile(prefix + f, files[f])
        report(data)

if __name__ == "__main__":
    main()
