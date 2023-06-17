# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2023, Intel Corporation

#
# src/test/tools/anonymous_mmap/Makefile -- Makefile for anonymous_mmap
#

TOP = ../../../..

TARGET = anonymous_mmap
OBJS = anonymous_mmap.o

LIBPMEM=y
LIBPMEMCOMMON=y

include $(TOP)/src/tools/Makefile.inc

TESTCONFIG=$(TOP)/src/test/testconfig.sh

all: max_dax_devices

max_dax_devices: $(TESTCONFIG) check_max_mmap.sh anonymous_mmap.static_nondebug
	@./check_max_mmap.sh
