# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_tx_locks/Makefile -- build obj_tx_locks unit test
#
TARGET = obj_tx_locks
OBJS = obj_tx_locks.o

LIBPMEMOBJ=y

include ../Makefile.inc

# one of the functions tests that deprecated functionality still works
# so we have to disable this warning to let it compile with -Werror
CFLAGS += -Wno-deprecated-declarations
