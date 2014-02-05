import ethercat

ethercat.initialise()

all_devs = ethercat.getAllDevices()
print "len of all_devs = %d"  % len(all_devs)

pdochoices = ethercat.getPdoEntryChoices(all_devs)
print "len of pdochoices = %d"  % len(pdochoices)

print "len of types_dict (filtered) = %d" % len(ethercat.types_dict)

print "len of types_choice = %d" % len(ethercat.types_choice)

