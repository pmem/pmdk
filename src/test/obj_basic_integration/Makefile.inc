# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016, Intel Corporation

#
# src/test/obj_basic_integration/Makefile.inc -- build obj_basic_integration,
#    obj_rpmem_basic_integration and obj_rpmem_basic_integration_multiple
#    unit tests
#

include ../Makefile.inc

../obj_basic_integration/obj_basic_integration:
	$(MAKE) -C ../obj_basic_integration all

all: ../obj_basic_integration/obj_basic_integration
