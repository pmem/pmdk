# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2017, Intel Corporation

#
# examples/logfile/Makefile -- build the Persistent Memory Development Kit examples
#
PROGS = addlog printlog

LIBS = -lpmemlog -lpmem -pthread

include ../../Makefile.inc

addlog: addlog.o

printlog: printlog.o

addlog.o printlog.o: logentry.h
