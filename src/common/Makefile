# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2022, Intel Corporation

#
# src/common/Makefile -- Makefile for common
#

LIBRARY_NAME = pmemcommon

include pmemcommon.inc

include ../Makefile.inc

CFLAGS += $(LIBNDCTL_CFLAGS) $(MINIASYNC_CFLAGS) -DPMEM2_USE_MINIASYNC=1
CFLAGS += -DUSE_LIBDL
