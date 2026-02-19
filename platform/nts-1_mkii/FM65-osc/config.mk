##############################################################################
# Configuration for Makefile
#

PROJECT := FM65-osc
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

UDEFS = -DPARAM_COUNT=10 -DFM=65 -DOP6 -DOPSIX -DSY77 -DBANK_COUNT=4 -DWFSIN16 -DSHAPE_LFO -DPEG -DEGLUT11 -DEGLUTX16 -DMOD16 -DFEEDBACK_COUNT=2 -DCUSTOM_ALGORITHM_COUNT=16 -DWAVE_PINCH
