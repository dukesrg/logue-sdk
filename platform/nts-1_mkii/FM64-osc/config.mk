##############################################################################
# Configuration for Makefile
#

PROJECT := FM64-osc
PROJECT_TYPE := osc

##############################################################################
# Sources
#

# C sources 
UCSRC = header.c

# C++ sources 
UCXXSRC = unit.cc

# List ASM source files here
UASMSRC = 

UASMXSRC = 

##############################################################################
# Include Paths
#

UINCDIR  = ../inc ../common/utils ../../inc

##############################################################################
# Library Paths
#

ULIBDIR = 

##############################################################################
# Libraries
#

ULIBS  = -lm

##############################################################################
# Macros
#

#UDEFS = -DPARAM_COUNT=10 -DFM=64 -DOP6 -DOPSIX -DSY77 -DBANK_COUNT=4 -DWFSIN16 -DSHAPE_LFO -DPEG -DEGLUT11 -DEGLUTX16
UDEFS = -DUNIT_TARGET_MODULE=k_unit_module_osc -DPARAM_COUNT=10 -DFM=64 -DOP6 -DOPSIX -DSY77 -DBANK_COUNT=4 -DWFGEN -DSHAPE_LFO -DPEG -DEGLUT11 -DEGLUTX16 -DFEEDBACK_COUNT=2
