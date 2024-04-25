##############################################################################
# Project Configuration
#

PROJECT := loguePAD2_synth
PROJECT_TYPE := synth

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

UINCDIR  = ../inc ../fastapprox/fastapprox/src

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

UDEFS = -DPARAM_COUNT=24 -DLOGUEPAD=2 -DLAYER_XFADE_RATE_BITS=6