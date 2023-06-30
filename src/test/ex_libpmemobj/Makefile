# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2023, Intel Corporation

#
# src/test/ex_libpmemobj/Makefile -- build ex_libpmemobj unittest
#

all: $(EXAMPLES)
	$(MAKE) -C $(EX_LIBPMEMOBJ)

include ../Makefile.inc

NCURSES := $(call check_package, ncurses)

EXAMPLES=$(EX_LIBPMEMOBJ)/pi \
	$(EX_LIBPMEMOBJ)/btree \
	$(EX_LIBPMEMOBJ)/map/mapcli \
	$(EX_LIBPMEMOBJ)/map/data_store \
	$(EX_LIBPMEMOBJ)/string_store/writer \
	$(EX_LIBPMEMOBJ)/string_store/reader \
	$(EX_LIBPMEMOBJ)/string_store_tx/writer \
	$(EX_LIBPMEMOBJ)/string_store_tx/reader \
	$(EX_LIBPMEMOBJ)/string_store_tx_type/writer \
	$(EX_LIBPMEMOBJ)/string_store_tx_type/reader

ifeq ($(NCURSES),y)
EXAMPLES += $(EX_LIBPMEMOBJ)/pminvaders/pminvaders
else
$(info NOTE: Skipping pminvaders test because ncurses is missing \
-- see src/examples/libpmemobj/pminvaders/README for details.)
endif
