# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

#
# src/test/ex_librpmem_fibonacci/Makefile -- prepare fibonacci librpmem example
# unittest
#

include ../../common.inc

ifeq ($(BUILD_RPMEM), y)
SCP_TO_REMOTE_NODES = y
SCP_TARGET = fibonacci
SCP_SRC_DIR = ../../examples/librpmem/fibonacci
endif

include ../Makefile.inc
