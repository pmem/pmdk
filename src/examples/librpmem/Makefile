# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2022, Intel Corporation

#
# examples/librpmem/Makefile -- build the librpmem examples
#
TOP := ../../..
include $(TOP)/src/common.inc

ifeq ($(BUILD_RPMEM), y)
PROGS = basic hello manpage
DIRS = fibonacci

LIBS = -lrpmem -pthread
else
$(info NOTE: Skipping librpmem examples because $(BUILD_RPMEM_INFO))
endif

include ../Makefile.inc

# Librpmem is deprecated.
# This flag allows to build tests, examples and benchmarks
# using rpmem despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations

basic: basic.o
hello: hello.o
manpage: manpage.o
