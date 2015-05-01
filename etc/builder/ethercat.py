
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
        "EP2338-0001", "EK1100", "EK1101", "EX260-SEC1", "EK1122", "EP1122-0001",
        "EL1124", "EP3314-0002", "EL3602", "NI 9144", "EL9410", "EP2624", 
        "EL2124", "EL4134", "EL9512", "EL3202-0010", "EL3104", "EL3602-0010",
        "EL2612", "EL2595", "EL3124"]
#The entries in the wiki with these names don't show up in the database
# EL9011 EL9080 EL9185 ZS2000-3712
# I23 has an EL2612 that is not in the list of supported modules

Debug = False


##################### module classes
class Sdo:
    '''An sdo to hold sdo entries for the sdo requests '''
    def __init__(self,name, slave_name, index):
        self.name = name
        self.index = index
        self.slave_name = slave_name
        self.entries = []
    def assignEntry(self, entry):
        self.entries.append(entry)
    def getSdoXml(self):
        o = "  <sdo name=\"%(name)s\""  % self.__dict__
        o = o + " slave=\"%(slave_name)s\""  % self.__dict__
        o = o + " index=\"0x%(index)x\" >\n" % self.__dict__
        for entry in self.entries:
            o = o + entry.getSdoEntryXml()
        o = o + "  </sdo>\n"
        return o

class SdoEntry:
    def __init__(self, parentsdo, name, asynparameter, description, subindex, bit_length):
        self.parentsdo = parentsdo
        self.name = name
        self.asynparameter = asynparameter
        self.description = description
        self.subindex = subindex
        self.bit_length = bit_length
        self.parentsdo.assignEntry(self)
    def getSdoEntryXml(self):
        o = "    <sdoentry"
        o = o + " description=\"%(description)s\"" % self.__dict__
        o = o + " subindex=\"0x%(subindex)x\" " % self.__dict__
        o = o + "bit_length=\"%(bit_length)d\" " % self.__dict__
        o = o + "asynparameter=\"%(asynparameter)s\"" % self.__dict__
        o = o + " />\n"
        return o

class PdoEntry:
    '''An entry in a devices's PDO'''
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
        return '        <' + f % self.__dict__ +  '/>\n'


class PaddingEntry(PdoEntry):
    count = 0
    ''' A padding entry in a PDO'''
    def __init__(self, name, index, bitlen):
        self.name = name
        PaddingEntry.count = PaddingEntry.count + 1
        self.name = (name + "Gap%d") % PaddingEntry.count
        self.count = PaddingEntry.count
        self.index = index
        self.bitlen = bitlen
        # dummy entries
        self.subindex = 0
        self.datatype = "BOOL"
        self.oversample = 0

    def generatePdoEntryXml(self):
        f =     'entry name="%(name)s" '
        f = f + 'index="0x%(index)08x" '
        f = f + 'subindex="0x%(subindex)08x" '
        f = f + 'bit_length="%(bitlen)d" '
        f = f + 'datatype="%(datatype)s" '
        f = f + 'oversample="%(oversample)d" '
        return '        <' + f % self.__dict__ +  '/>\n'

class Pdo:
    '''A device's pdo entry
       direction can be "Tx" for input from slaves 
       or "Rx" for output to slaves
       syncmanager can be None for non-default pdos
    '''
    def __init__(self, name, index, syncmanager, rxtx):
        self.name = name
        self.index = index
        self.entries = []
        self.syncmanager = syncmanager
        self.rxtx = rxtx
        self.defaultPdo = True

    def addEntry(self, pdoentry):
        if pdoentry:
            self.entries.append(pdoentry)
            pdoentry.parent = self
    def generatePdoXml(self):
        ''' xml description for the ethercat scanner '''
        o = "      <pdo name=\"%(name)s\"" % self.__dict__ 
        o = o + " index=\"0x%(index)08x\">\n" % self.__dict__
        for entry in self.entries:
            o = o + entry.generatePdoEntryXml()
        o = o + "      </pdo>\n"
        
        return o
    def getPdoSignals(self):
        ''' list of sample signals for Generic ADCs '''
        return [ x.makeParamName() for x in self.entries ]

class SyncManager:
    ''' A device's sync manager '''
    def __init__(self, index, direction, watchdog):
        self.index = index
        self.direction = direction
        self.watchdog = watchdog
        self.pdos = []
        self.pdo_index_list = []
        self.number = "-1" # placeholder number

    def pdosWithoutDuplicates(self):
        # remove duplicates
        pdo_index_list = []
        checked_pdos = []
        for pdo in self.pdos:
            if not pdo.index in pdo_index_list:
                checked_pdos.append(pdo)
                pdo_index_list.append(pdo.index)
        return checked_pdos

    def generateSyncManagerXml(self):
        ''' xml description for the ethercat scanner '''
        o = '    <sync index="%(number)s" ' % self.__dict__
        o = o + ' dir="%(direction)s" '% self.__dict__
        o = o + ' watchdog="%(watchdog)d">\n' % self.__dict__
        for pdo in self.pdosWithoutDuplicates():
            o = o + pdo.generatePdoXml()
        o = o + "    </sync>\n"
        return o

    def assignPdo(self, pdo):
        if not pdo.index in self.pdo_index_list:
            self.pdo_index_list.append(pdo.index)
            self.pdos.append(pdo)
        # else:
        #     print "Duplicate pdo skipped (index = %d, name %s)" % \
        #         (pdo.index, pdo.name)
        
class EthercatDevice:
    ''' An EtherCAT device description, 
        parsed from the manufacturer's description'''
    def __init__(self, type, vendor, product, revision ):
        self.type = type
        self.vendor = vendor
        self.product = product
        self.revision = revision
        self.syncmanagers = {}
        self.txpdos = []
        self.rxpdos = []
        self.assign_activate = None

    def addSyncManager(self, sm):
        if not sm:
            return
        self.syncmanagers[int(sm.number)] = sm

    def getPdoByIndex(self, pdo_index):
        for pdo in self.txpdos:
            if pdo.index == pdo_index:
                return pdo
        for pdo in self.rxpdos:
            if pdo.index == pdo_index:
                return pdo
        return None

    def transferAssignments(self, elem):
        """transfer pdo assignments from an EthercatChainElem object"""
        if Debug:
            print "transferAssignments for elem %s" % elem.portname
        if not elem.processedAssignedPdos:
            for smnumber in range(4):
                for pdo_index in elem.assignedPdos[smnumber]:
                    self.assignPdo(smnumber, pdo_index)
            elem.processedAssignedPdos = True

    def assignPdo(self, syncmanager_number, pdo_index):
        pdo = self.getPdoByIndex(pdo_index)
        if syncmanager_number in self.syncmanagers \
           and pdo is not None:
            self.syncmanagers[syncmanager_number].assignPdo(pdo)

    def addPdo(self, pdo ):
        if not pdo:
            return
        if Debug:
            if not pdo.syncmanager in self.syncmanagers:
                print "Syncmanager %d not found in device. " % pdo.syncmanager
            a = (pdo.syncmanager, self.syncmanagers[pdo.syncmanager].direction)
            print "Adding pdo for syncmanager %s, direction %s" % a
        if pdo.rxtx == "tx": 
            #assert self.syncmanagers[pdo.syncmanager].direction == "Inputs"
            self.txpdos.append(pdo)
        else:
            #assert self.syncmanagers[pdo.syncmanager].direction == "Outputs"
            assert pdo.rxtx == "rx" # output
            self.rxpdos.append(pdo)
        if pdo.syncmanager:
            self.syncmanagers[int(pdo.syncmanager)].assignPdo(pdo)

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
        # for pdo in self.txpdos:
        #     o = o + pdo.generatePdoXml()
        # for pdo in self.rxpdos:
        #     o = o + pdo.generatePdoXml()
        for sm in self.syncmanagers:
            o = o + self.syncmanagers[sm].generateSyncManagerXml()
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
        # one set per synch manager
        self.assignedPdos = {0: set(),
                             1: set(),
                             2: set(),
                             3: set() }
        # record whether device descriptions were processed for 
        # non default PDOs
        self.processedAssignedPdos = True
        # list of sdos - specific to a chain element, not to the device type
        self.assignedSdos = []

    def assignPdo(self, smnumber, pdo_index):
        assert smnumber in [0,1,2,3]
        self.assignedPdos[smnumber].add(pdo_index)
        self.processedAssignedPdos = False

    def assignSdo(self, sdo):
        self.assignedSdos.append(sdo)

    def getDeviceDescription(self):
        key = parseTypeRev(self.type_rev)
        self.device = all_dev_descriptions[key]
        if not self.processedAssignedPdos:
            self.device.transferAssignments(self)

    def getSdoXml(self):
        o = ""
        for sdo in self.assignedSdos:
            o = o + sdo.getSdoXml()
        return o

        
class EthercatChain:
    '''a collection of slaves with positions and driver information'''
    def __init__(self):
        self.chain = {} # hash of EthercatChainElem
        self.chainlist = [] # list of positions
        self.dev_descriptions = dict()

    def setDevice(self, chainelem):
        assert( chainelem.position not in self.chainlist )
        self.chain[chainelem.position] = chainelem
        self.chainlist.append(chainelem.position)

    def getDeviceDescriptions(self):
        ''' populate the chain-elements' device descriptions from the
            descriptions en ethercat.dev_descriptions
        '''
        for pos in self.chain:
            self.chain[pos].getDeviceDescription()
        for pos in self.chain:
            key = parseTypeRev(self.chain[pos].type_rev)
            self.dev_descriptions[key] = all_dev_descriptions[key]

    def generateChainXml(self):
        # from itertools import chain
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
        
    def generateSdorequestsXml(self):
        o = '<sdorequests>\n'
        for chainelem in [ self.chain[position] for position in self.chainlist ]:
            o = o + chainelem.getSdoXml()
        o = o + '</sdorequests>\n'
        return o

    def generateMasterXml(self):
        assert self.dev_descriptions , "device descriptions not populated. should call getDeviceDescriptions"
        o = "<scanner>\n"
        o = o + "<devices>\n" 
        for key, dev_description in self.dev_descriptions.iteritems():
            o = o + dev_description.generateDeviceXml()
        o = o + "</devices>\n"
        o = o + self.generateChainXml()
        o = o + self.generateSdorequestsXml()
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
        stypes = getPdoEntryChoices(all_dev_descriptions)
        types_dict = filteredDescriptions(all_dev_descriptions)
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
        
def parsePadding(entry):
    ''' parse a padding pdo entry'''
    name = ""
    try:
        name = entry.xpathEval("Name")[0].content
    except:
        pass
    index = parseInt(entry.xpathEval("Index")[0].content)
    bitlen = parseInt(entry.xpathEval("BitLen")[0].content)
    return PaddingEntry(name, index, bitlen)

def parsePdoEntry(entry, oversample):
    '''parse an xml entry node and return a PdoEntry object'''
    # is this padding?
    padding = False
    try:
        # padding can have a name but no subindex
        subindex = parseInt(entry.xpathEval("SubIndex")[0].content)
        # some pdos entries don't specify a datatype, ignore
        datatype = entry.xpathEval("DataType")[0].content
    except:
        padding = True
    if padding:
        return parsePadding(entry)
    # some pdo entries are just padding with no name, ignore
    try:
        name = entry.xpathEval("Name")[0].content
    except:
        return None
    index = parseInt(entry.xpathEval("Index")[0].content)
    bitlen = parseInt(entry.xpathEval("BitLen")[0].content)
    return  PdoEntry(name, index, subindex, bitlen, datatype, oversample)


def parsePdo(pdoNode, os, rxtx):
    '''read pdo description from an xml pdo node'''
        
    # some pdos don't have a name, ignore
    if not pdoNode.xpathEval("Name"):
        return None
    name = pdoNode.xpathEval("Name")[0].content
    index = parseInt(pdoNode.xpathEval("Index")[0].content)
    if Debug:
        a = (name, index, syncmanager)
        print "Creating pdo name %s index %x syncmanager %s" % a
    defaultPdo = False
    #Some Pdos have a sync manager - those are marked defaultPdo
    sm = pdoNode.xpathEval("@Sm")
    if not sm:
        defaultPdo = False
        syncmanager = None
    else:
        syncmanager = sm[0].content
        defaultPdo = True
    pdo = Pdo(name,index, syncmanager, rxtx)
    pdo.defaultPdo = defaultPdo
    for entry in pdoNode.xpathEval("Entry"):
        pdo.addEntry( parsePdoEntry(entry, index in os) )
    return pdo

def parseSyncManager(smNode):
    ''' read sync manager description from an xml sm node'''
    watchdog = 0
    direction = smNode.content # "Inputs", "Outputs", "MBoxOut", etc
    index = parseInt(smNode.xpathEval("@StartAddress")[0].content)
    if Debug:
        a = (index, direction, watchdog)
        print "found syncmanager index=%x direction=%s watchdog=%d" % a
    sm = SyncManager(index, direction, watchdog)
    return sm

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
        smCount = 0
        for smNode in devNode.xpathEval("Sm"):
            sm = parseSyncManager(smNode)
            sm.number = str(smCount)
            smCount = smCount + 1
            device.addSyncManager(sm)
        for txpdo in devNode.xpathEval("TxPdo"):
            pdo = parsePdo(txpdo, oversampling, "tx")  
            device.addPdo( pdo ) 
        for rxpdo in devNode.xpathEval("RxPdo"):
            pdo = parsePdo(rxpdo, oversampling, "rx")  
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
