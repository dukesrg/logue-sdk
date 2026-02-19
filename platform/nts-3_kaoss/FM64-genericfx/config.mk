##############################################################################
# Configuration for Makefile
#

PROJECT := FM64-genericfx
PROJECT_TYPE := genericfx

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

UINCDIR  = ../../nts-1_mkii/inc ../common/utils ../../inc

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

#UDEFS = -DUNIT_TARGET_MODULE=k_unit_module_genericfx -DPARAM_COUNT=8 -DFM=64 -DOP6 -DOPSIX -DSY77 -DBANK_COUNT=1 -DWFGEN -DPEG -DEGLUT11 -DEGLUTX16 -DFEEDBACK_COUNT=2
UDEFS = -DUNIT_TARGET_MODULE=k_unit_module_genericfx -DPARAM_COUNT=8 -DFM=64 -DOP6 -DOPSIX -DSY77 -DBANK_COUNT=1 -DWFSIN16 -DPEG -DEGLUT11 -DEGLUTX16 -DFEEDBACK_COUNT=2

