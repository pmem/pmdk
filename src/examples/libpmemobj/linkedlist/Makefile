# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016, Intel Corporation
#
# examples/libpmemobj/linkedlist/Makefile -- build the tail queue examples
#
PROGS = fifo

LIBS = -lpmemobj -lpmem -pthread

include ../../Makefile.inc

fifo: fifo.o

fifo.o: pmemobj_list.h
