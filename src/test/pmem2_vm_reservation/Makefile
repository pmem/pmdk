# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

#
# src/test/pmem2_vm_reservation/Makefile -- build pmem2_vm_reservation
#                                           unit tests
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

INCS += -I$(TOP)/src/libpmem2
TARGET = pmem2_vm_reservation
OBJS += pmem2_vm_reservation.o\
	ut_pmem2_utils.o\
	ut_pmem2_source.o\
	ut_pmem2_setup.o

LIBPMEM2=internal-debug

include ../Makefile.inc
