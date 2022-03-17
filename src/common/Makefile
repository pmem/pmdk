# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2022, Intel Corporation

#
# src/common/Makefile -- Makefile for common
#

LIBRARY_NAME = pmemcommon

include pmemcommon.inc

include ../Makefile.inc

CFLAGS += $(LIBNDCTL_CFLAGS)
CFLAGS += -DUSE_LIBDL
# Librpmem is deprecated.
# This flag allows to build tests, examples and benchmarks
# using rpmem despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
