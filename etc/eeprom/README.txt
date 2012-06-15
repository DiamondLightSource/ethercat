
Programming a NI 9144 backplane EEPROM
======================================

This directory holds images of the EEPROM on NI9144 backplanes for these configurations

NI9144 rev 1 ..........ni_9144_rev_1.bin 
NI9144 rev 2 ..........ni_9144_rev_2.bin 


        slot 1  slot 2  slot 3  slot4   slot5   slot6   slot7   slot8
rev 1   NI9125                          
rev 2   NI9125  NI9125  NI9125                  
rev 3   NI9215  NI9234  NI9263                  
rev 4   NI9239                          

To program a slave, use this command:

ethercat sii_write -m <master_no> -p <slave_no> <eeprom-image.bin>

e.g. 
ethercat sii_write -m0 -p8 ni_9144_rev_1.bin

The 'ethercat' tool (from etherlab) is installed under 

/dls_sw/prod/R3.14.11/support/etherlab/<etherlab_release>/opt/<arch>/etherlab/bin

<etherlab_release> is 1-5-pre or 1-5-1 (june 2012)
<arch> is usually linux-x86
