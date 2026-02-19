##############################################################################
# Configuration for Makefile
#

PROJECT := FM69-osc
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

UINCDIR  = ../inc ../common/utils

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

UDEFS = -DPARAM_COUNT=10 -DFM=69 -DOP6 -DOPSIX -DSY77 -DBANK_COUNT=5 -DWFSIN16 -DSHAPE_LFO -DPEG -DKIT_MODE -DEGLUT11 -DEGLUTX16
