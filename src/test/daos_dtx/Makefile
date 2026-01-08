# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

#
# src/test/daos_dtx/Makefile -- build a daos_dtx validation executable
#
PMDK_BUILD_DAOS_TESTS ?= n
ifeq ($(PMDK_BUILD_DAOS_TESTS),y) # compile conditionally

# check for required env vars
ifeq ($(DAOS_SRC_PATH),)
$(error DAOS_SRC_PATH is required)
endif

ifeq ($(DAOS_INSTALL_PATH),)
$(error DAOS_INSTALL_PATH is required)
endif

TARGET = daos_dtx
OBJS = daos_dtx.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

LIBPMEMOBJ=y

include ../Makefile.inc
INCS += -I$(DAOS_SRC_PATH)/src/utils/ddb/ -I$(DAOS_INSTALL_PATH)/include/
LIBS += -L$(DAOS_INSTALL_PATH)/lib64/daos_srv/
DYNAMIC_LIBS += -lddb

else
# required to support general targets like `cstyle`
include ../Makefile.inc
$(info Set PMDK_BUILD_DAOS_TESTS=y to build DAOS tests)
endif # PMDK_BUILD_DAOS_TESTS == y
