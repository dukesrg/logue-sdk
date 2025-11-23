PROJECT := $(lastword $(subst /, , $(dir $(abspath $(lastword $(MAKEFILE_LIST))))))
PROJECT_TYPE := $(lastword $(subst -, , $(PROJECT)))
UNIT_NAME := \"$(firstword $(subst -, , $(PROJECT)))\"
CSRC = header.c
CXXSRC = unit.cc
UINCDIR = ../../inc ../../ext/zip/src
ULIBS = -lm -lc
UDEFS = -DUNIT_NAME=$(UNIT_NAME) -DUNIT_TARGET_MODULE=k_unit_module_$(PROJECT_TYPE) -DPARAM_COUNT=8 -DBANK_COUNT=4 -DGENRE_COUNT=8
USE_LDOPT = --allow-multiple-definition
USE_CWARN = -W -Wall -Wextra -Wcast-align
USE_CXXWARN = $(USE_CWARN)
#UDEFS += -DPERFMON_ENABLE
#DEBUG = 1