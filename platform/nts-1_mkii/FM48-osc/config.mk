##############################################################################
# Configuration for Makefile
#

PROJECT := FM48-osc
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

UDEFS = -DPARAM_COUNT=10 -DFM=48 -DOP4 -DOPSIX -DSY77 -DBANK_COUNT=4 -DWF8 -DSHAPE_LFO -DPEG -DEGLUT11 -DEGLUTX16