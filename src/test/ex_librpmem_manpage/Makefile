# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

#
# src/test/ex_librpmem_manpage/Makefile -- build manpage librpmem example
# unittest
#

include ../../common.inc

ifeq ($(BUILD_RPMEM), y)
SCP_TO_REMOTE_NODES = y
SCP_TARGET = manpage
SCP_SRC_DIR = ../../examples/librpmem
endif

include ../Makefile.inc

