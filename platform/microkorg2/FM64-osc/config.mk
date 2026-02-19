PROJECT := $(lastword $(subst /, , $(dir $(abspath $(lastword $(MAKEFILE_LIST))))))
PROJECT_TYPE := $(lastword $(subst -, , $(PROJECT)))
UNIT_NAME := \"$(firstword $(subst -, , $(PROJECT)))\"
CSRC = header.c
CXXSRC = unit.cc
UINCDIR = ../../inc ../common/utils ../common/dsp ../../nts-1_mkii/inc
ULIBS = -lm -lc
#UDEFS = -DUNIT_NAME=$(UNIT_NAME) -DUNIT_TARGET_MODULE=k_unit_module_$(PROJECT_TYPE) -DPARAM_COUNT=10 -DFM=64 -DOP6 -DOPSIX -DSY77 -DBANK_COUNT=32 -DWFGEN -DPEG -DEGLUT12 -DFEEDBACK_COUNT=2
UDEFS = -DUNIT_NAME=$(UNIT_NAME) -DUNIT_TARGET_MODULE=k_unit_module_$(PROJECT_TYPE) -DPARAM_COUNT=10 -DFM=64 -DOP6 -DOPSIX -DSY77 -DPEG -DEGLUT12 -DFEEDBACK_COUNT=2
UDEFS += -DWFSIN16
UDEFS += -DBANK_COUNT=4
USE_LDOPT = --allow-multiple-definition
USE_CWARN = -W -Wall -Wextra -Wcast-align
USE_CXXWARN = $(USE_CWARN)
#UDEFS += -DPERFMON_ENABLE
#DEBUG = 1