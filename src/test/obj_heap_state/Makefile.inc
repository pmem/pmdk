# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2022, Intel Corporation

#
# src/test/obj_heap_state/Makefile.inc -- build obj_heap_state unittest
#

../obj_heap_state/obj_heap_state:
	$(MAKE) -C ../obj_heap_state all

all: ../obj_heap_state/obj_heap_state

include ../Makefile.inc
