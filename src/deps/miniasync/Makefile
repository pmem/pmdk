# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

#
# src/deps/miniasync/makefile -- build miniasync
#

# WARNING: This file is created in the pmdk repository
# and doesn't belong to the pmem/miniasync repository.
#

dir = build

$(dir):
	mkdir -p ./build

configure: | $(dir)
	cd ./build && cmake .. -DCMAKE_BUILD_TYPE=Release

all: configure
	mkdir -p ./build
	cd ./build && make

clean: configure
	cd ./build && make clean

clobber: | $(dir)
	rm -r ./build
