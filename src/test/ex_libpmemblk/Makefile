# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2016, Intel Corporation

#
# src/test/ex_libpmemblk/Makefile -- build ex_libpmemblk unittest
#

all: $(EXAMPLES)
	$(MAKE) -C $(EX_LIBPMEMBLK)

include ../Makefile.inc

EXAMPLES=$(EX_LIBPMEMBLK)/assetdb/asset_load \
	$(EX_LIBPMEMBLK)/assetdb/asset_checkin \
	$(EX_LIBPMEMBLK)/assetdb/asset_checkout \
	$(EX_LIBPMEMBLK)/assetdb/asset_list

