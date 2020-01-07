# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2016, Intel Corporation

#
# src/test/ex_libpmem/Makefile -- build ex_libpmem unittest
#

all: $(EXAMPLES)
	$(MAKE) -C $(EX_LIBPMEM)

include ../Makefile.inc

EXAMPLES=$(EX_LIBPMEM)/simple_copy \
	$(EX_LIBPMEM)/full_copy

