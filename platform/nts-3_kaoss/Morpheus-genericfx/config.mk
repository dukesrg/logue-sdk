PROJECT := $(lastword $(subst /, , $(dir $(abspath $(lastword $(MAKEFILE_LIST))))))
PROJECT_TYPE := $(lastword $(subst -, , $(PROJECT)))
UNIT_NAME := \"$(firstword $(subst -, , $(PROJECT)))\"
UCSRC = header.c
UCXXSRC = unit.cc
UINCDIR = ../../inc ../common/utils ../common/dsp
ULIBS = -lm
UDEFS = -DUNIT_NAME=$(UNIT_NAME) -DUNIT_TARGET_MODULE=k_unit_module_$(PROJECT_TYPE) -DPARAM_COUNT=7 -DFORMAT_PCM8 -DSAMPLE_GUARD -DLFO_MODE_COUNT=4 -DLFO_WAVEFORM_COUNT=69
#UDEFS += -DBPM_SYNC_SUPPORTED
UDEFS += -Wcast-align
#UDEFS += -DPERFMON_ENABLE