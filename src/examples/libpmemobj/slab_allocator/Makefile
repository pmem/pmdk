# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017, Intel Corporation

#
# examples/libpmemobj/slab_allocator/Makefile -- build the slab_allocator example
#
TOP := $(dir $(lastword $(MAKEFILE_LIST)))../../../../
include $(TOP)/src/common.inc

PROGS = main
LIBRARIES = slab_allocator

LIBS = -lpmemobj

include ../../Makefile.inc

libslab_allocator.o: slab_allocator.o

main: main.o slab_allocator.o
