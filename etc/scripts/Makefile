TOP = ../..
include $(TOP)/configure/CONFIG

# include standard rules
include $(TOP)/configure/RULES

../../builder/iocbuilder_descriptions.pkl: ../../builder/build_iocbuilder_descriptions.py
	dls-python ../../builder/build_iocbuilder_descriptions.py

../../builder/ethercat_descriptions.pkl: ../../builder/build_ethercat_descriptions.py
	dls-python ../../builder/build_ethercat_descriptions.py

build: ../diamond_types.py ../../builder/iocbuilder_descriptions.pkl ../../builder/ethercat_descriptions.pkl
	../diamond_types.py > slave-types.txt
	cp slave-types.txt ../slave-types.txt

uninstall:
	-rm ../../builder/*.pkl

# # rule for building slave types list
# slave-types.txt:
# 	../diamond_types.py > $@
