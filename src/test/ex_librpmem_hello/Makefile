# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

#
# src/test/ex_librpmem_hello/Makefile -- prepare hello librpmem example
# unittest
#

include ../../common.inc

ifeq ($(BUILD_RPMEM), y)
SCP_TO_REMOTE_NODES = y
SCP_TARGET = hello
SCP_SRC_DIR = ../../examples/librpmem
endif

include ../Makefile.inc
