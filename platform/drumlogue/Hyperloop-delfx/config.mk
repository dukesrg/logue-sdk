##############################################################################
# Project Configuration
#

PROJECT := Hyperloop_delay
PROJECT_TYPE := delfx

##############################################################################
# Sources
#

# C sources
CSRC = header.c

# C++ sources
CXXSRC = unit.cc

# List ASM source files here
ASMSRC = 

ASMXSRC = 

##############################################################################
# Include Paths
#

UINCDIR  = ../inc

##############################################################################
# Library Paths
#

ULIBDIR = 

##############################################################################
# Libraries
#

ULIBS  = -lm
ULIBS += -lc

##############################################################################
# Macros
#

UDEFS = -DPARAM_COUNT=24 -DUNIT_TARGET_MODULE=k_unit_module_delfx
