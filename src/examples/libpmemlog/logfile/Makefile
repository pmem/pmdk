# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# examples/logfile/Makefile -- build the Persistent Memory Development Kit examples
#
PROGS = addlog printlog

LIBS = -lpmemlog -lpmem -pthread

include ../../Makefile.inc

# Libpmemlog is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemlog despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations

addlog: addlog.o

printlog: printlog.o

addlog.o printlog.o: logentry.h
