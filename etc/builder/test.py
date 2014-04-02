import ethercat
import sys
import unittest

class TestEthercatDescriptions(unittest.TestCase):
    
    def setUp(self):
        ethercat.initialise()
    def testContents(self):
        self.assertTrue( len(ethercat.getAllDevices()) > 0 )


class TestEthercatChain(unittest.TestCase):

    def setUp(self):
        ethercat.initialise()

    def testChain(self):
        chain1 = ethercat.EthercatChain()
        elem1 = ethercat.EthercatChainElem("EK1101 rev 0x00110000",0,"ERIO.0",0)
        chain1.setDevice(elem1)
        chain1.getDeviceDescriptions()
        xml = chain1.generateChainXml()
        self.assertTrue( xml )
        self.assertTrue( len(xml) > 0 )
        

if __name__ == "__main__":
    #import sys;sys.argv = ['', 'Test.testName']

    # suite1 = unittest.TestLoader().loadTestsFromTestCase(TestEthercatDescriptions)
    # suite2 = unittest.TestLoader().loadTestsFromTestCase(TestEthercatChain)
    # unittest.TextTestRunner(verbosity=2).run(suite1)
    # unittest.TextTestRunner(verbosity=2).run(suite2)
    unittest.main()
