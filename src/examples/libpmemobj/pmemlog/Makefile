# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2023, Intel Corporation

#
# examples/libpmemobj/pmemlog/Makefile -- build obj implementations of pmemlog
#
PROGS = obj_pmemlog obj_pmemlog_macros obj_pmemlog_minimal obj_pmemlog_simple

LIBS = -lpmemobj -lpmem -pthread

include ../../Makefile.inc

# Libpmemlog is deprecated.
# This flag allows to build tests and examples
# using pmemlog despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations

obj_pmemlog: obj_pmemlog.o

obj_pmemlog_macros: obj_pmemlog_macros.o

obj_pmemlog_minimal: obj_pmemlog_minimal.o

obj_pmemlog_simple: obj_pmemlog_simple.o
