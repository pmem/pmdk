# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2022, Intel Corporation

#
# examples/librpmem/fibonacci/Makefile -- build the fibonacci example
#
PROGS = fibonacci

LIBS = -lrpmem -lpmem -pthread

include ../../Makefile.inc

# Librpmem is deprecated.
# This flag allows to build tests, examples and benchmarks
# using rpmem despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
fibonacci: fibonacci.o
