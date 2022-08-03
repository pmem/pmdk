# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2022, Intel Corporation

#
# src/test/obj_heap_interrupt/Makefile.inc -- build obj_heap_interrupt unit tests
#

../obj_heap_interrupt/obj_heap_interrupt:
	$(MAKE) -C ../obj_heap_interrupt all

all: ../obj_heap_interrupt/obj_heap_interrupt

include ../Makefile.inc
