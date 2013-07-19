from iocbuilder.modules.asyn import Asyn
from iocbuilder import Device, IocDataStream, AutoSubstitution
from iocbuilder.arginfo import makeArgInfo, Simple, Ident, Choice
from . import ethercat
import os

# These devices are used directly, while the others are loaded as part of
# other devices
__all__ = ['EthercatMaster', 'EthercatSlave', 'GenericADC','GenericADCTemplate','MasterTemplate']

class GenericADCTemplate(AutoSubstitution):
    TemplateFile = 'gadc.template'

class MasterTemplate(AutoSubstitution):
    TemplateFile = 'MASTER.template'

class SlaveTemplate(AutoSubstitution):
    TemplateFile = 'SLAVE.template'

# script for scanner start-up, used in EthercatMaster.writeScannerStartup
SCANNER_STARTUP_TEXT ="""#!/bin/sh
cd "$(dirname $0)"
%(scanner)s -q %(expanded_chain)s %(socket_path)s
"""

class EthercatMaster(Device):
    '''An EtherCAT Master device for the iocbuilder
      Keeps a 'chain' member with a list of slaves in the bus
      '''
    Dependencies = ( Asyn, )
    LibFileList = [ 'ecAsyn' ]
    DbdFileList = [ 'ecAsyn' ]

    def __init__(self, socket, max_message = 100000):
        self.__super.__init__()
        self.socket = socket
        self.max_message = max_message
        self.chain = ethercat.EthercatChain()
        self.chainfile = IocDataStream("chain.xml")
        self.expandedfile = IocDataStream("expanded.xml")
        self.scannerf = IocDataStream("scanner.sh",mode=0555)
        self.dev_descriptions = []

    def Initialise(self):
        print 'ecAsynInit("%(socket)s", %(max_message)d)' % self.__dict__
        self.getDeviceDescriptions()
        self.expandedfile.write( self.generateMasterXml() )
        self.writeScannerStartup()

    ArgInfo = makeArgInfo(__init__,
        socket = Simple("scanner socket path", str),
        max_message = Simple("max scanner message size", int))

    def writeScannerStartup(self):
        scanner_path = os.path.join(ethercat.etcdir,'../bin/linux-x86_64/scanner')
        self.scannerf.write(SCANNER_STARTUP_TEXT % dict(
                scanner = os.path.realpath(scanner_path),
                expanded_chain = "./" + self.expandedfile.name,
                socket_path = self.socket))

    def setSlave(self, slave):
        self.chain.setSlave(slave)

    def getDeviceDescriptions(self):
        self.chain.getDeviceDescriptions()
    
    def generateChainXml(self):
        return self.chain.generateChainXml()

    def generateMasterXml(self):
        return self.chain.generateMasterXml()

class EthercatSlave(Device):
    ''' An EtherCAT slave, usually an entry in a chain '''
    Dependencies = ( Asyn, )
    LibFileList = [ 'ecAsyn' ]
    DbdFileList = [ 'ecAsyn' ]
    # Set of allocated positions to avoid accidential duplication
    __Positions = set()
    _class_initialised = False

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
        position = Simple("slave position in ethercat chain, or serial number of format DCS00001234", str),
        type_rev = Choice("Device type and revision",ethercat.types_choice),
        name = Simple("slave's asyn port name", str),
        oversample = Simple("slave's oversampling rate, e.g. on an EL4702 oversample=100 bus freq 1kHz gives 100kHz samples",int))

class GenericADC(Device):
    ''' A generic ADC signal'''
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
        pdoentry = Choice("parameter in the form pdo_name.entry_name", ethercat.pdo_entry_choices),
        cycle = Simple("cycle parameter in the form pdo_name.entry_name", str)
        )
