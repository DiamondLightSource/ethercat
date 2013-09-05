
import os

### module variables
builder_dir = os.path.dirname(__file__)
etcdir = os.path.realpath(os.path.join(builder_dir,'..'))

#all device descriptions in etc/xml
all_dev_descriptions = None
stypes = None
initialised = False

#result of filteredDescription
types_dict = None
types_choice = None

# entries valid for a GenericAdc parameter
pdo_entry_choices = None

# filter for ethercat device types supported at DLS, according to the entries in 
# http://www.cs.diamond.ac.uk/cgi-bin/wiki.cgi/SupportedEtherCATModules
diamondFilter = [
        "EL1502", "EP3174-0002", "EL2624", "EK1100-0030", "EL2024-0010",
        "EL3202", "EL1014", "EP4374-0002", "EL4732",
        "EL9505", "EP2624-0002", "EL1014-0010", "EX260-SEC3", "EX260-SEC2", 
        "EX260-SEC4", "EL3314", "EP3204-0002", "EL3702",
        "EL2024", "EP4174-0002", "EX250-SEN1-X156", "EP2338-0002", 
        "EP2338-0001", "EK1100", "EX260-SEC1", "EK1122", "EP1122-0001",
        "EL1124", "EP3314-0002", "EL3602", "NI 9144", "EL9410", "EP2624", 
        "EL2124", "EL4134", "EL9512", "EL3202-0010", "EL3104", "EL3602-0010",
        "EL2612"]
#The entries in the wiki with these names don't show up in the database
# EL9011 EL9080 EL9185 ZS2000-3712
# I23 has an EL2612 that is not in the list of supported modules

##################### module classes

class PdoEntry:
    '''A device pdo's entry'''
    def __init__(self, name, index, subindex, bitlen, datatype,oversample):
        self.name = name
        self.index = index
        self.subindex = subindex
        self.bitlen = bitlen
        self.datatype = datatype
        self.oversample = oversample
        
    # The sample is a pdo entry in the format pdo_name.entry_name
    def makeParamName(self):
        ''' sample signal for Generic ADCs '''
        o = "%s.%s" % (self.parent.name, self.name)
        return o.replace(" ","")

    def generatePdoEntryXml(self):
        ''' xml description for the ethercat scanner '''
        f =     'entry name="%(name)s" '
        f = f + 'index="0x%(index)08x" '
        f = f + 'subindex="0x%(subindex)08x" '
        f = f + 'bit_length="%(bitlen)d" '
        f = f + 'datatype="%(datatype)s" '
        f = f + 'oversample="%(oversample)d" '
        return '      <' + f % self.__dict__ +  '/>\n'

class Pdo:
    '''A device's pdo entry'''
    def __init__(self, name, index, direction):
        self.name = name
        self.index = index
        self.direction = direction
        self.entries = []
    def addEntry(self, pdoentry):
        if pdoentry:
            self.entries.append(pdoentry)
            pdoentry.parent = self
    def generatePdoXml(self):
        ''' xml description for the ethercat scanner '''
        o = '    <sync index="0" dir="0" watchdog="0">\n    '
        o = o + "<pdo dir=\"%(direction)d\" " % self.__dict__
        o = o + "name=\"%(name)s\"" % self.__dict__ 
        o = o + " index=\"0x%(index)08x\">\n" % self.__dict__
        for entry in self.entries:
            o = o + entry.generatePdoEntryXml()
        o = o + "    </pdo>\n"
        o = o + "    </sync>\n"
        return o
    def getPdoSignals(self):
        ''' list of sample signals for Generic ADCs '''
        return [ x.makeParamName() for x in self.entries ]
        
class EthercatDevice:
    ''' An EtherCAT device description, typically parsed from the manufacturer's description'''
    def __init__(self, type, vendor, product, revision ):
        self.type = type
        self.vendor = vendor
        self.product = product
        self.revision = revision
        self.txpdos = []
        self.rxpdos = []
        self.assign_activate = None
    def addPdo(self, pdo ):
        if not pdo:
            return
        if pdo.direction == 1:
            self.txpdos.append(pdo)
        else:
            self.rxpdos.append(pdo)

    def generateDeviceXml(self):
        ''' xml description for the ethercat scanner '''
        o = "  <!-- parsed from file %(file)s -->\n" % self.__dict__ 
        o = o + '  <device name="%(type)s" ' % self.__dict__
        o = o + 'vendor="0x%(vendor)08x" ' % self.__dict__ 
        o = o + 'product="0x%(product)08x" ' % self.__dict__
        o = o + 'revision="0x%(revision)08x"' % self.__dict__
        if self.assign_activate:
            o = o + ' dcactivate="0x%(assign_activate)08x"' % self.__dict__
        o = o + ">\n"
        for pdo in self.txpdos:
            o = o + pdo.generatePdoXml()
        for pdo in self.rxpdos:
            o = o + pdo.generatePdoXml()
        o = o + "  </device>\n"
        return o
    def getDeviceSignals(self):
        "all the device's signals"
        r = []
        for pdo in self.txpdos:
            r.extend(pdo.getPdoSignals())
        for pdo in self.rxpdos:
            r.extend(pdo.getPdoSignals())
        return r

    def getTypicalDeviceSignals(self):
        """filter to typical ADC signals. The criteria is: 
           signals whose name includes 'value' or for the ni 9144, 
           signals whose name have 'in'"""
        allSignals = self.getDeviceSignals()
        signals = [item for item in allSignals if "value" in item.lower()]
        if "ni 9144" in self.type.lower():
            signals.extend([item for item in allSignals \
                            if "in" in item.lower()])
        return signals

class EthercatChainElem:
    # Set of allocated positions to avoid accidential duplication
    __Positions = set()
    _class_initialised = False

    '''an ethercat device in a chain position'''
    def __init__(self, type_rev, position, portname, oversample):
        self.type_rev = type_rev
        self.type, self.revision = parseTypeRev(type_rev)
        self.position = position
        self.portname = portname
        self.oversample = oversample 
        self.device = None
        assert position not in self.__Positions, \
            "Slave position %d already taken" % position
        self.__Positions.add(position)


class EthercatChain:
    '''a collection of slaves with positions and driver information'''
    def __init__(self):
        self.chain = {} # hash of EthercatChainElem
        self.chainlist = []
        self.dev_descriptions = dict()

    def setDevice(self, chainelem):
        assert( chainelem.position not in self.chainlist )
        self.chain[chainelem.position] = chainelem
        self.chainlist.append(chainelem.position)

    def getDeviceDescriptions(self):
        ''' populate the chain-elements' device descriptions from the
            descriptions en ethercat.dev_descriptions
        '''
        global all_descriptions
        reqs = set()   # set of devices in chain
        missingDevices = False
        for elem in [self.chain[position] for position in self.chainlist]:
            reqs.add(parseTypeRev(elem.type_rev))
            if not elem.device:
                missingDevices = True
        if not missingDevices:
            return
        expected_len = len(reqs)
        self.dev_descriptions = dict()        
        for key, dev in all_dev_descriptions.iteritems():
            if key in reqs:
                self.dev_descriptions[key] = dev
                missingDevices = False
                for elem in [self.chain[position] for position in self.chainlist]:
                    if elem.device:
                        continue
                    if (elem.type, elem.revision) == key:
                        elem.device = dev
                    else:
                        missingDevices = True
                if not missingDevices:
                    break
        assert( expected_len == len(self.dev_descriptions) ), \
            "The following modules are not listed in ethercat/etc/xml: \n%s" % \
                [x for x in reqs if x not in self.dev_descriptions]

    def generateChainXml(self):
        from itertools import chain
        o = '<chain>\n'
        #produces a list of tuples (position, type_rev, portname, oversample)
        for chainelem in [ self.chain[position] for position in self.chainlist ]:
            o = o + '<device type_name="%(type)s"'  % chainelem.__dict__
            o = o + ' revision="0x%(revision)08x"'  % chainelem.__dict__
            o = o + ' position="%(position)s"'      % chainelem.__dict__
            o = o + ' name="%(portname)s"'          % chainelem.__dict__
            if chainelem.oversample != 0:
                o = o + ' oversample="%(oversample)d"' % chainelem.__dict__
            o = o + " />\n"
        o = o + "</chain>\n"
        return o

    def generateMasterXml(self):
        assert self.dev_descriptions , "device descriptions not populated. should call getDeviceDescriptions"
        o = "<scanner>\n"
        o = o + "<devices>\n" 
        for key, dev_description in self.dev_descriptions.iteritems():
            o = o + dev_description.generateDeviceXml()
        o = o + "</devices>\n"
        o = o + self.generateChainXml()
        o = o + "</scanner>\n"
        return o

############### module functions

def initialise(forceInitialisation = False):
    global all_dev_descriptions
    global stypes
    global types_choice
    global types_dict
    global pdo_entry_choices
    if not initialised or forceInitialisation:
        all_dev_descriptions = None
        stypes = None
        all_dev_descriptions = getAllDevices()
        stypes = getPdoEntryChoices(dev_descriptions)
        types_dict = filteredDescriptions(dev_descriptions)
        types_choice = ["%s rev 0x%08x" % key \
                for key in sorted(types_dict.keys())]
        pdo_entry_choices =  ["%s : %s rev 0x%08x" % k for k in getPdoEntryChoices(types_dict) ]

def filteredDescriptions(dev_descriptions = None,filter = None):
    '''returns a dictionary of devices filtered by typename''' 
    if filter == None:
        filter = diamondFilter
    filtered_descriptions = {}
    for key in dev_descriptions:
        if key in filtered_descriptions.keys():
            print "Duplicate key", key
            continue
        typename = key[0]
        if typename in filter:
            filtered_descriptions[key] = dev_descriptions[key]
    return filtered_descriptions

def parseInt(text):
    if text.startswith("#x") or text.startswith("0x"):
        return int(text.replace("#x", ""), 16)
    else:
        return int(text)
 
def parsePdoEntry(entry, oversample):
    '''parse an xml entry node and return a PdoEntry object'''
    # some pdo entries are just padding with no name, ignore
    try:
        name = entry.xpathEval("Name")[0].content
    except:
        return None
    try:
        # or padding can have a name but no subindex
        subindex = parseInt(entry.xpathEval("SubIndex")[0].content)
        # some pdos entries don't specify a datatype, ignore
        datatype = entry.xpathEval("DataType")[0].content
    except:
        return None
    index = parseInt(entry.xpathEval("Index")[0].content)
    bitlen = parseInt(entry.xpathEval("BitLen")[0].content)
    return  PdoEntry(name, index, subindex, bitlen, datatype, oversample)

def parsePdo(pdoNode, d, os):
    '''read pdo description from an xml pdo node'''
    # some pdos don't have a sync manager assigment
    # not supported by EtherLab driver
    if not pdoNode.xpathEval("@Sm"):
        return None
    # some pdos don't have a name, ignore
    if not pdoNode.xpathEval("Name"):
        return None
    name = pdoNode.xpathEval("Name")[0].content
    index = parseInt(pdoNode.xpathEval("Index")[0].content)
    pdo = Pdo(name,index, d)
    for entry in pdoNode.xpathEval("Entry"):
        pdo.addEntry( parsePdoEntry(entry, index in os) )
    return pdo

def getDescriptions(filename):
    '''return a dictionary of device descriptions in the file'''
    import libxml2
    doc = libxml2.parseFile(filename)
    vendor = parseInt(doc.xpathEval("//Vendor/Id")[0].content)
    dev_dictionary = {}
    for devNode in doc.xpathEval("//Device"):
        try:
            type = devNode.xpathEval("Type")[0].content
            product = parseInt(devNode.xpathEval("Type/@ProductCode")[0].content)
            revision = parseInt(devNode.xpathEval("Type/@RevisionNo")[0].content)
        except:
            continue
        key = (type, revision)
        oversampling = set()
        device = EthercatDevice(type, vendor, product, revision)
        aa = devNode.xpathEval("Dc/OpMode/AssignActivate")
        if len(aa):
            for e in aa:
                if parseInt(e.content) != 0:
                    device.assign_activate = parseInt(aa[0].content)
                    break
        device.file = filename
        for dcmode in devNode.xpathEval("Dc/OpMode/Sm/Pdo[@OSFac]"):
            oversampling.add(parseInt(dcmode.content))
        for txpdo in devNode.xpathEval("TxPdo"):
            pdo = parsePdo(txpdo, 1, oversampling)  
            device.addPdo( pdo ) 
        for rxpdo in devNode.xpathEval("RxPdo"):
            pdo = parsePdo(rxpdo, 0, oversampling)  
            device.addPdo(pdo) 
        dev_dictionary[key] = device
    return dev_dictionary

def getAllDevices():
   '''create a dictionary of possible devices from the xml description files
      The keys are of the form (typename, revision) and the entries are whole device
      description objects
   '''
   global dev_descriptions
   base = os.path.join(etcdir,'xml')
   if not all_dev_descriptions:
       dev_descriptions = dict()
       for f in os.listdir(base):
           if f.endswith("xml"):
               filename = os.path.join(base, f)
               for key, dev in getDescriptions(filename).iteritems():
                   typename = key[0]
                   revision = key[1]
                   dev_descriptions[key] = dev
   return dev_descriptions 

def getPdoEntryChoices(all_devices):
    'typical pdo entry choices for Generic ADC sample signals'
    global stypes
    if not stypes:
        stypes = []
        for dev in all_devices.itervalues():
            for s in dev.getTypicalDeviceSignals():
                stypes.append( (s, dev.type, dev.revision) )
        stypes = sorted(stypes, key=lambda s: "%s rev 0x%08x" % (s[1],s[2]) )
    return stypes

def parseTypeRev(type_rev):
    "parse type rev and return tuple (type, revision)"
    devtype = type_rev.split(" rev ")[0]
    revision = int(type_rev.split(" rev ")[1], 16)
    return (devtype, revision)
