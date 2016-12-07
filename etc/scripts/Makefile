TOP = ../..
include $(TOP)/configure/CONFIG

# include standard rules
include $(TOP)/configure/RULES

build: ../diamond_types.py
	../diamond_types.py > slave-types.txt
	cp slave-types.txt ../slave-types.txt

# # rule for building slave types list 
# slave-types.txt:
# 	../diamond_types.py > $@

