# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# examples/libpmemobj/pminvaders/Makefile -- build the pminvaders examples
#
TOP := $(dir $(lastword $(MAKEFILE_LIST)))../../../../

include $(TOP)/src/common.inc

NCURSES := $(call check_package, ncurses)
ifeq ($(NCURSES),y)
PROGS = pminvaders pminvaders2
else
$(info NOTE: Skipping pminvaders and pminvaders2 because ncurses is missing \
-- see src/examples/libpmemobj/pminvaders/README for details.)
endif

LIBS = -lpmemobj -lpmem -pthread

ifeq ($(NCURSES),y)
LIBS += $(shell $(PKG_CONFIG) --libs ncurses)
endif

include ../../Makefile.inc

pminvaders: pminvaders.o
pminvaders2: pminvaders2.o
