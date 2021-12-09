#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2021, Intel Corporation
#

#
# install-txt2man.sh - installs txt2man
#

set -e

git clone https://github.com/mvertes/txt2man.git
cd txt2man

# txt2man v1.7.0
git checkout txt2man-1.7.0

make -j$(nproc)
sudo make -j$(nproc) install prefix=/usr
cd ..
rm -rf txt2man
