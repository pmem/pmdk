# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

#
# src/test/ex_librpmem_basic/Makefile -- prepare basic librpmem example
# unittest
#

include ../../common.inc

ifeq ($(BUILD_RPMEM), y)
SCP_TO_REMOTE_NODES = y
SCP_TARGET = basic
SCP_SRC_DIR = ../../examples/librpmem
endif

include ../Makefile.inc
