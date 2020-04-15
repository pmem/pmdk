#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation

#
# install-libfabric.sh - installs a customized version of libfabric
#

set -e

OS=$1

# Keep in sync with requirements in src/common.inc.
libfabric_ver=1.4.2
libfabric_url=https://github.com/ofiwg/libfabric/archive
libfabric_dir=libfabric-$libfabric_ver
libfabric_tarball=v${libfabric_ver}.zip
wget "${libfabric_url}/${libfabric_tarball}"
unzip $libfabric_tarball

cd $libfabric_dir

# XXX HACK HACK HACK
# Disable use of spin locks in libfabric.
#
# spinlocks do not play well (IOW at all) with cpu-constrained environments,
# like GitHub Actions, and this leads to timeouts of some PMDK's tests.
# This change speeds up pmempool_sync_remote/TEST28-31 by a factor of 20-30.
#
perl -pi -e 's/have_spinlock=1/have_spinlock=0/' configure.ac
# XXX HACK HACK HACK

./autogen.sh
./configure --prefix=/usr --enable-sockets
make -j$(nproc)
make -j$(nproc) install

cd ..
rm -f ${libfabric_tarball}
rm -rf ${libfabric_dir}
