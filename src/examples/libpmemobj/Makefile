# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# examples/libpmemobj/Makefile -- build the libpmemobj examples
#
PROGS = manpage btree pi lists setjmp buffons_needle_problem
DIRS = pminvaders string_store string_store_tx string_store_tx_type hashmap \
	tree_map pmemobjfs map libart linkedlist list_map slab_allocator queue

LIBS = -lpmemobj -lpmem -pthread -lm -pthread

include ../Makefile.inc

map: hashmap tree_map
pmemobjfs: map

manpage: manpage.o
btree: btree.o
pi: pi.o
lists: lists.o
buffons_needle_problem: buffons_needle_problem.o

setjmp: CFLAGS += -O2
setjmp: setjmp.o
