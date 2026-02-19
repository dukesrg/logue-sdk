##############################################################################
# Configuration for Makefile
#

PROJECT := FM68-osc
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

UDEFS = -DPARAM_COUNT=10 -DFM=68 -DOP6 -DOPSIX -DSY77 -DBANK_COUNT=1 -DWFSIN16 -DSHAPE_LFO -DPEG -DEGLUT13 -DEGLUTX16
