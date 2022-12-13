# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2022, Intel Corporation

#
# src/test/obj_basic_integration/Makefile.inc -- build obj_basic_integration
# unit tests
#

include ../Makefile.inc

../obj_basic_integration/obj_basic_integration:
	$(MAKE) -C ../obj_basic_integration all

all: ../obj_basic_integration/obj_basic_integration
