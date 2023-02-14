# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# examples/libpmemblk/Makefile -- build the libpmemblk examples
#
PROGS = manpage
DIRS = assetdb

LIBS = -lpmemblk -lpmem -pthread

include ../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations

manpage: manpage.o
