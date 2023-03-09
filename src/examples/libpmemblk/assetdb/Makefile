# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# examples/libpmemblk/assetdb/Makefile -- build the assetdb example
#
PROGS = asset_load asset_list asset_checkout asset_checkin

LIBS = -lpmemblk -lpmem -pthread

include ../../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations

asset_load: asset_load.o

asset_list: asset_list.o

asset_checkout: asset_checkout.o

asset_checkin: asset_checkin.o

asset_load.o asset_list.o asset_checkout.o asset_checkin.o: asset.h
