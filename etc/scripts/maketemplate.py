#!/usr/bin/python2.4

# need system python for libxml2 support with xpath

import libxml2
import sys

reqs = set()
verbose = False

def parseInt(text):
    if text.startswith("#x") or text.startswith("0x"):
        return int(text.replace("#x", ""), 16)
    else:
        return int(text)

longin_text = """
record(longin, "$(DEVICE):%(name)s")
{
  field("DTYP", "asynInt32")
  field("INP",  "@asyn($(PORT))%(command)s")
  field("SCAN", "$(SCAN)")
}
"""

longout_text = """
record(longout, "$(DEVICE):%(name)s")
{
  field("DTYP", "asynInt32")
  field("OUT",  "@asyn($(PORT))%(command)s")
  field("OMSL", "supervisory")
}
"""
bi_text = """
record(bi, "$(DEVICE):%(name)s")
{
  field("DTYP", "asynInt32")
  field("INP",  "@asyn($(PORT))%(command)s")
  field("SCAN", "$(SCAN)")
  field("ZNAM", "OFF")
  field("ONAM", "ON")
}
"""
bo_text = """
record(bo, "$(DEVICE):%(name)s")
{
  field("DTYP", "asynInt32")
  field("OUT",  "@asyn($(PORT))%(command)s")
  field("OMSL", "supervisory")
  field("ZNAM", "OFF")
  field("ONAM", "ON")
}
"""

def makeTemplate(longin, longout, bi, bo, output):
    print "Generating template file %s" % output
    f = file(output, "w")
    for l in ["AL_STATE", "ERROR_FLAG"]:
        recname = l.replace(".", ":")
        print >> f,longin_text % {"name": recname, "command": l}
    for l in longin:
        recname = l.replace(".", ":")
        print >> f, longin_text % {"name": recname, "command": l}
    for l in bi:
        recname = l.replace(".", ":")
        print >> f, bi_text % {"name": recname, "command": l}
    for l in longout:
        recname = l.replace(".", ":")
        print >> f, longout_text % {"name": recname, "command": l}
    for l in bo:
        recname = l.replace(".", ":")
        print >> f, bo_text % {"name": recname, "command": l}
    f.close()

def getPdoName(node):
    name = node.xpathEval("Name")[0].content
    name = name.replace(" ", "")
    return name

def hasEntryName(node):
    try:
        name= node.xpathEval("Name")[0].content
    except:
        return False
    return True

def getEntryName(node):
    name= node.xpathEval("Name")[0].content
    name= name.replace(" ", "")
    return name

def parseFile(filename, output, list=False):
    doc = libxml2.parseFile(filename)
    vendor = parseInt(doc.xpathEval("//Vendor/Id")[0].content)
    for device in doc.xpathEval("//Device"):
        try:
            devtype = device.xpathEval("Type")[0].content
            product = parseInt(device.xpathEval("Type/@ProductCode")[0].content)
            revision = parseInt(device.xpathEval("Type/@RevisionNo")[0].content)
        except:
            continue
        # key = (vendor, product, revision)
        key = (devtype, revision)
        if list:
            print "%s 0x%08x" % key
            continue

        oversampling = set()
        
        if key in reqs:

            longin = []
            longout = []
            bi = []
            bo = []
            
            for dcmode in device.xpathEval("Dc/OpMode/Sm/Pdo[@OSFac]"):
                oversampling.add(parseInt(dcmode.content))
            for txpdo in device.xpathEval("TxPdo"):
                for entry in txpdo.xpathEval("Entry"):
                    # some pdo entries are just padding with no name, ignore
                    if hasEntryName(entry):
                        datatype = entry.xpathEval("DataType")[0].content
                        if datatype == "BOOL":
                            bi.append(getPdoName(txpdo) + "." + getEntryName(entry) )
                        else:
                            longin.append(getPdoName(txpdo) + "." + getEntryName(entry) )
                    elif verbose:
                        print "Ignoring entry in pdo %s" % getPdoName(txpdo)
            for rxpdo in device.xpathEval("RxPdo"):
                for entry in rxpdo.xpathEval("Entry"):
                    # some pdo entries are just padding with no name, ignore
                    if hasEntryName(entry):
                        datatype = entry.xpathEval("DataType")[0].content
                        if datatype == "BOOL":
                            bo.append(getPdoName(rxpdo) + "." + getEntryName(entry) )
                        else:
                            longout.append(getPdoName(rxpdo) + "." + getEntryName(entry) )
                    elif verbose:
                        print "Ignoring entry in pdo %s" % getPdoName(txpdo)

            makeTemplate(longin, longout, bi, bo, output)

def usage(progname):
    print "%s: Make EPICS template for EtherCAT device" % progname
    print "Usage:"
    print "   %s -h  Shows this usage message" % progname
    print "   %s -b <xml-base-dir> -l  Lists the devices in the database" % progname
    print """
   %s -b <xml-base-dir> -d <device-type> -r <rev-no> -o output-file
       Generates a template in <output-file> for the given device and revision.
       rev-no must be input as a hex number, e.g. 0x00100000
       """ % progname

if __name__ == "__main__":
    import getopt
    base = None
    devtype = None
    revision = None
    output = None
    list = False
    
    try:
        optlist, args = getopt.getopt(sys.argv[1:], 'hlb:d:r:o:v',
                    ['help','list','base=','device-type=','rev-no=',
                     'output=','verbose'])
    except getopt.GetoptError, err:
        print str(err)
        usage(sys.argv[0])
        sys.exit(2)
    for o,a in optlist:
        if o in ('-h', '--help'):
            usage(sys.argv[0])
            sys.exit()
        elif o in ('-l', '--list'):
            list = True
        elif o in ('-b', '--base'):
            base = a
        elif o in ('-d','--device-type'):
            devtype = a
        elif o in ('-r','--rev-no'):
            revision = int(a,16)
        elif o in ('-v','--verbose'):
            verbose = True
        elif o in ('-o','--output'):
            output = a
        else:
            usage(sys.argv[0])
            sys.exit(1)
    if not base:
        print "No base specified"
        usage(sys.argv[0])
        sys.exit(1)
    elif verbose:
        print "base=%s" % base
    if not list:
        if not devtype :
            print "No devtype specified"
            usage(sys.argv[0])
            sys.exit(1)
        elif verbose:
            print "devtype=%s" % devtype
        if  revision == None:
            print "No revision specified"
            usage(sys.argv[0])
            sys.exit(1)
        elif verbose:
            print "revision=%s" % revision
        if not output:
            print "No output specified"
            usage(sys.argv[0])
            sys.exit(1)
        elif verbose:
            print "output=%s" % output

        reqs.add((devtype, revision))
        if verbose:
            for obj in reqs:
                print "Searching device %s, revision 0x%08x" % obj

    import os
    for f in os.listdir(base):
        if f.endswith("xml"):
            filename = os.path.join(base, f)
            if verbose:
                print "Parsing %s" % filename
            parseFile(filename, output, list)



# loads chain description, outputs complete config file...
