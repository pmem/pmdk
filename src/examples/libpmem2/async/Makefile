# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# examples/libpmem2/async/Makefile -- build the async example
#
PROGS = async

LIBS = -lpmem2 -lpthread

include ../../Makefile.inc

async: main.o mover.o runtime.o future.o
