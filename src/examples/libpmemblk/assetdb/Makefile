# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2016, Intel Corporation

#
# examples/libpmemblk/assetdb/Makefile -- build the assetdb example
#
PROGS = asset_load asset_list asset_checkout asset_checkin

LIBS = -lpmemblk -lpmem -pthread

include ../../Makefile.inc

asset_load: asset_load.o

asset_list: asset_list.o

asset_checkout: asset_checkout.o

asset_checkin: asset_checkin.o

asset_load.o asset_list.o asset_checkout.o asset_checkin.o: asset.h
