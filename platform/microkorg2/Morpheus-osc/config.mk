PROJECT := $(lastword $(subst /, , $(dir $(abspath $(lastword $(MAKEFILE_LIST))))))
PROJECT_TYPE := $(lastword $(subst -, , $(PROJECT)))
UNIT_NAME := \"$(firstword $(subst -, , $(PROJECT)))\"
CSRC = header.c
CXXSRC = unit.cc
UINCDIR = ../../inc ../common/utils ../common/dsp
ULIBS = -lm
ULIBS += -lc
UDEFS = -DUNIT_NAME=$(UNIT_NAME) -DUNIT_TARGET_MODULE=k_unit_module_$(PROJECT_TYPE) -DPARAM_COUNT=10 -DFORMAT_PCM16 -DSAMPLE_GUARD -DLFO_MODE_COUNT=4 -DLFO_WAVEFORM_COUNT=159
USE_LDOPT = --allow-multiple-definition