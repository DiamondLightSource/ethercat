#!/bin/env dls-python

from xml.dom.minidom import parse
import sys

doc = parse(sys.argv[1])
chain = doc.getElementsByTagName("chain")[0]
chain.tagName = "components"
chain.setAttribute("arch", "linux-x86")
m = doc.createElement("ethercat.EthercatMaster")
m.setAttribute("name", "ECATM")
m.setAttribute("socket","/tmp/socket")
chain.insertBefore(m, chain.firstChild)
for device in chain.getElementsByTagName("device"):
    type_name = device.getAttribute("type_name")
    device.removeAttribute("type_name")
    revision = device.getAttribute("revision")
    device.removeAttribute("revision")
    device.setAttribute("type_rev", "%s rev %s" % (type_name, revision))
    device.tagName = "ethercat.EthercatSlave"
    device.setAttribute("master", "ECATM")
print doc.toxml()



