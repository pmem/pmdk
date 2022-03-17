# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2022, Intel Corporation

#
# src/core/Makefile -- Makefile for core
#

LIBRARY_NAME = pmemcore

include pmemcore.inc

include ../Makefile.inc

# Librpmem is deprecated.
# This flag allows to build tests, examples and benchmarks
# using rpmem despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
