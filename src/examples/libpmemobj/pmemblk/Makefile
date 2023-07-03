# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2023, Intel Corporation

#
# examples/libpmemobj/pmemblk/Makefile -- build obj implementations of pmemblk
#
PROGS = obj_pmemblk

LIBS = -lpmemobj -lpmem -pthread

include ../../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests and examples
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations

obj_pmemblk: obj_pmemblk.o
