#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# install-valgrind.sh - installs valgrind for persistent memory
#

set -e

git clone https://github.com/pmem/valgrind.git
cd valgrind
# valgrind v3.15 with pmemcheck
git checkout c27a8a2f973414934e63f1e94bc84c0a580e3840
./autogen.sh
./configure
make -j$(nproc)
make -j$(nproc) install
cd ..
rm -rf valgrind
