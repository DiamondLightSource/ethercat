TOP=../..

include $(TOP)/configure/CONFIG

# ------------------------
# Build an IOC Application
# ------------------------

PROD_IOC = niRev4

# niRev4.dbd will be installed into <top>/dbd
DBD += niRev4.dbd

# niRev4.dbd will be created from these files
niRev4_DBD += base.dbd
niRev4_DBD += asyn.dbd
niRev4_DBD += ecAsyn.dbd

# niRev4_registerRecordDeviceDriver.cpp will be created
# niRev4.dbd
niRev4_SRCS += niRev4_registerRecordDeviceDriver.cpp

# These two lines are needed for non-vxWorks builds, such as Linux
niRev4_SRCS_DEFAULT += niRev4Main.cpp
niRev4_SRCS_vxWorks += -nil-

# Add locally compiled object code
#niRev4_SRCS +=

# The following adds object code from base/src/vxWorks
niRev4_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# This line says that this IOC Application depends on the
# xxx Support Module
#niRev4_LIBS += xxx

# We need to link this IOC Application against the EPICS Base libraries
niRev4_LIBS += ecAsyn
niRev4_LIBS += asyn
niRev4_LIBS += $(EPICS_BASE_IOC_LIBS)

# ---------------------------------------------------
DATA += scanner.xml

include $(TOP)/configure/RULES

scanner.xml: ../chain.xml
	echo "Executing rule to create scanner.xml"
	$(ETHERCAT)/etc/scripts/expandChain.py $(ETHERCAT)/etc/xml $< > $@
