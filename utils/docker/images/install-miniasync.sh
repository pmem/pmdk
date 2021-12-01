#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

#
# install-miniasync.sh - installs miniasync library
#

set -e

git clone https://github.com/pmem/miniasync.git
cd miniasync

# XXX: as miniasync in actively in development,
# we need to consider better solution
# 2021.12.1 Merge pull request #20 from pmem/coverity-badge
git checkout 2453713fd223c154dff205a19aede6699364d222
mkdir build && cd build

cmake ..
make -j$(nproc)
make -j$(nproc) install

cd ../..
rm -rf ./miniasync
