PROJECT := $(lastword $(subst /, , $(dir $(abspath $(lastword $(MAKEFILE_LIST))))))
PROJECT_TYPE := $(lastword $(subst -, , $(PROJECT)))
UNIT_NAME := $(firstword $(subst -, , $(PROJECT)))
UCXXSRC = ../../src/morpheus.cc
UINCDIR = ../../inc
UDEFS = -DUSER_TARGET_MODULE=k_user_module_osc -DFORMAT_PCM12 -DSAMPLE_GUARD -DLFO_AXES_COUNT=2 -DLFO_MODE_COUNT=8 -DLFO_WAVEFORM_COUNT=159
