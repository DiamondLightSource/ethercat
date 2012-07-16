from iocbuilder.modules.asyn import Asyn
from iocbuilder import Device, IocDataStream, AutoSubstitution
from iocbuilder.arginfo import makeArgInfo, Simple, Ident, Choice
import os 

# These devices are used directly, while the others are loaded as part of
# other devices
__all__ = ['EthercatMaster', 'EthercatSlave', 'GenericADC','GenericADCTemplate','MasterTemplate']

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

etcdir = os.path.dirname(__file__)

class GenericADCTemplate(AutoSubstitution):
    TemplateFile = 'gadc.template'

class MasterTemplate(AutoSubstitution):
    TemplateFile = 'MASTER.template'

def parseInt(text):
    if text.startswith("#x") or text.startswith("0x"):
        return int(text.replace("#x", ""), 16)
    else:
        return int(text)

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


# script for scanner start-up, used in EthercatMaster.writeScannerStartup
SCANNER_STARTUP_TEXT ="""#!/bin/sh
cd "$(dirname $0)"
%(scanner)s %(expanded_chain)s %(socket_path)s
"""

class EthercatMaster(Device):
    '''An EtherCAT Master'''
    Dependencides = ( Asyn, )
    LibFileList = [ 'ecAsyn' ]
    DbdFileList = [ 'ecAsyn' ]

    def __init__(self, socket, max_message = 100000):
        self.__super.__init__()
        self.socket = socket
        self.max_message = max_message
        self.chain = {}
        self.chainfile = IocDataStream("chain.xml")
        self.expandedfile = IocDataStream("expanded.xml")
        self.scannerf = IocDataStream("scanner.sh",mode=0555)
        self.dev_descriptions = []

    def Initialise(self):
        print 'ecAsynInit("%(socket)s", %(max_message)d)' % self.__dict__
        self.getDeviceDescriptions()
        self.chainfile.write( self.generateChainXml() )
        self.expandedfile.write( self.generateMasterXml() )
        self.writeScannerStartup()

    ArgInfo = makeArgInfo(__init__,
        socket = Simple("scanner socket path", str),
        max_message = Simple("max scanner message size", int))

    def writeScannerStartup(self):
        scanner_path = os.path.join(etcdir,'../bin/linux-x86/scanner')
        self.scannerf.write(SCANNER_STARTUP_TEXT % dict(
                scanner = os.path.realpath(scanner_path),
                expanded_chain = "./" + self.expandedfile.name,
                socket_path = self.socket))

    def setSlave(self, slave):
        assert( slave.position not in self.chain.keys() )
        self.chain[slave.position] = slave 

    def getDeviceDescriptions(self):
        ''' populate the slaves' device descriptions from the
            descriptions en EthercatSlave._all_types_dict
        '''
        reqs = set()   # set of devices in chain
        missingDevices = False
        for position,slave in self.chain.iteritems():
            reqs.add((slave.type,slave.revision))
            if not slave.device:
                missingDevices = True
        if not missingDevices:
            return
        expected_len = len(reqs)
        self.dev_descriptions = dict()
        for key, dev in EthercatSlave._all_types_dict.iteritems():
            if key in reqs:
                self.dev_descriptions[key] = dev
                missingDevices = False
                for position, slave in self.chain.iteritems():
                    if slave.device:
                        continue
                    if (slave.type, slave.revision) == key:
                        slave.device = dev
                    else:
                        missingDevices = True
                if not missingDevices:
                    break
        assert( expected_len == len(self.dev_descriptions) )
    
    def generateChainXml(self):
        o = '<chain>\n'
        for pos, slave in self.chain.iteritems():
            o = o + '<device type_name="%(type)s"'  % slave.__dict__
            o = o + ' revision="0x%(revision)08x"'  % slave.__dict__
            o = o + ' position="%(position)d"'      % slave.__dict__ 
            o = o + ' name="%(name)s"'              % slave.__dict__
            if slave.oversample != 0:
                o = o + ' oversample="%(oversample)d"' % slave.__dict__
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
   base = os.path.join(etcdir,'xml')
   dev_descriptions = {}
   for f in os.listdir(base):
       if f.endswith("xml"):
           filename = os.path.join(base, f)
           for key, dev in getDescriptions(filename).iteritems():
               typename = key[0]
               revision = key[1]
               dev_descriptions[key] = dev
   return dev_descriptions 

def filteredDescriptions(dev_descriptions = None,filter = None):
    '''returns a dictionary of devices filtered by typename''' 
    if dev_descriptions == None:
        dev_descriptions = getAllDevices()
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

class EthercatSlave(Device):
    ''' An EtherCAT slave, usually an entry in a chain '''
    Dependencies = ( Asyn, )
    LibFileList = [ 'ecAsyn' ]
    DbdFileList = [ 'ecAsyn' ]
    # Set of allocated positions to avoid accidential duplication
    __Positions = set()
    _all_types_dict = getAllDevices()
    _types_dict = filteredDescriptions(_all_types_dict)
    _types_choice = ["%s rev 0x%08x" % key \
                for key in sorted(_types_dict.keys())]
    def __init__(self, master, position, name, type_rev, oversample = 0):
        self.__super.__init__()
        self.master = master
        assert position not in self.__Positions, \
            "Slave position %d already taken" % position
        self.position = position
        self.__Positions.add(position)
        self.name = name
        self.type_rev = type_rev
        self.type = type_rev.split(" rev ")[0]
        self.revision = int(type_rev.split(" rev ")[1], 16)
        self.oversample = oversample
        self.device = None
        self.master.setSlave(self)

    def getAllSignals(self):
        if self.device == None:
            self.master.getDeviceDescriptions()
        assert( self.device != None)
        return self.device.getDeviceSignals()

    def getTypicalSignals(self):
        if self.device == None:
            self.master.getDeviceDescriptions()
        assert( self.device != None)
        return self.device.getTypicalDeviceSignals()

    ArgInfo = makeArgInfo(__init__,
        master = Ident("ethercat master device", EthercatMaster),
        position = Simple("slave position in ethercat chain", int),
        type_rev = Choice("Device type and revision",_types_choice),
        name = Simple("slave's asyn port name", str),
        oversample = Simple("slave's oversampling rate",int))

def getPdoEntryChoices():
    'typical pdo entry choices for Generic ADC sample signals'
    stypes = []
    for dev in EthercatSlave._types_dict.itervalues():
        for s in dev.getTypicalDeviceSignals():
            stypes.append( (s, dev.type, dev.revision) )
    return sorted(stypes, key=lambda s: "%s rev 0x%08x" % (s[1],s[2]) )

class GenericADC(Device):
    ''' A generic ADC signal'''
    _PdoEntryChoices = ["%s : %s rev 0x%08x" % k for k in getPdoEntryChoices() ] 
    def __init__(self, slave, channel, pdoentry, cycle=None):
        self.__super.__init__()
        self.slave = slave
        self.channel = channel
        self.sample = pdoentry.split(" : ")[0]
        self.cycle = cycle
        self.slave_name = slave.name
        assert( self.sample in self.slave.getAllSignals() )

    def Initialise_FIRST(self):
        if self.cycle:
            print 'ADC_Ethercat_Sampler("%(slave_name)s",' % self.__dict__ \
                  + '%(channel)d,"%(sample)s","%(cycle)s")' % self.__dict__
        else:
            print 'ADC_Ethercat_Sampler("%(slave_name)s",%(channel)d,"%(sample)s")' \
                  % self.__dict__
    ArgInfo = makeArgInfo(__init__, 
        slave = Ident("ethercat slave", EthercatSlave),
        channel = Simple("channel id number", int),
        pdoentry = Choice("parameter in the form pdo_name.entry_name", _PdoEntryChoices),
        cycle = Simple("cycle parameter in the form pdo_name.entry_name", str)
        )

