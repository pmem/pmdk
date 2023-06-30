# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2023, Intel Corporation

#
# src/libpmempool/Makefile -- Makefile for libpmempool
#

LIBRARY_NAME = pmempool
LIBRARY_SO_VERSION = 1
LIBRARY_VERSION = 0.0

INCS += -I$(TOP)/src/libpmemobj
INCS += -I$(TOP)/src/libpmem2

include ../core/pmemcore.inc
include ../common/pmemcommon.inc

SOURCE +=\
	libpmempool.c\
	check.c\
	check_bad_blocks.c\
	check_backup.c\
	check_pool_hdr.c\
	check_sds.c\
	check_util.c\
	pool.c\
	replica.c\
	feature.c\
	sync.c\
	transform.c\
	rm.c

include ../Makefile.inc

CFLAGS += $(LIBNDCTL_CFLAGS)
LIBS += -pthread -lpmem $(LIBDL) $(LIBNDCTL_LIBS)
CFLAGS += -DUSE_LIBDL
