# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2016, Intel Corporation

#
# examples/libpmemobj/pmemlog/Makefile -- build obj implementations of pmemlog
#
PROGS = obj_pmemlog obj_pmemlog_macros obj_pmemlog_minimal obj_pmemlog_simple

LIBS = -lpmemobj -lpmem -pthread

include ../../Makefile.inc

obj_pmemlog: obj_pmemlog.o

obj_pmemlog_macros: obj_pmemlog_macros.o

obj_pmemlog_minimal: obj_pmemlog_minimal.o

obj_pmemlog_simple: obj_pmemlog_simple.o
