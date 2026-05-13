PROJECT := $(lastword $(subst /, , $(dir $(abspath $(lastword $(MAKEFILE_LIST))))))
PROJECT_TYPE := $(lastword $(subst -, , $(PROJECT)))
UNIT_NAME := \"$(firstword $(subst -, , $(PROJECT)))\"
CSRC = header.c
CXXSRC = unit.cc
UINCDIR = ../../inc ../../inc/TinySoundFont
UINCDIR += ../../common 
ULIBS = -lm -lc
UDEFS = -DUNIT_NAME=$(UNIT_NAME) -DUNIT_TARGET_MODULE=k_unit_module_$(PROJECT_TYPE) -DALLOW_DEPRECATED_FUNCTIONS_API_2_1_0 -DPARAM_COUNT=5
USE_CWARN = -W -Wall -Wextra -Wcast-align
USE_CXXWARN = $(USE_CWARN)
#UDEFS += -DPERFMON_ENABLE
#DEBUG = 1