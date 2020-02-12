# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2016, Intel Corporation

#
# src/test/obj_list/Makefile.inc -- build obj_list* unittest
#

../obj_list/obj_list:
	$(MAKE) -C ../obj_list all

all: ../obj_list/obj_list

include ../Makefile.inc
