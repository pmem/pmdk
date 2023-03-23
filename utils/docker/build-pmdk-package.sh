#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation

#
# build-pmdk-package.sh - Script for building pmdk package.
#

#FULL_PATH=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
#PMDK_PATH="${FULL_PATH}/../.."

set -eo pipefail

#
# build_pmdk_package -- build pmdk package depending on the Linux distribution.
#
function build_pmdk_package {
	cd $WORKDIR
	echo "********** make package **********"
	echo $OS
	if [ $OS = opensuse-leap ] || [ $OS = rockylinux ]; then
		make rpm EXPERIMENTAL=y BUILD_PACKAGE_CHECK=n PMEM2_INSTALL=y
	elif [ $OS = ubuntu ]; then
		make dpkg EXPERIMENTAL=y BUILD_PACKAGE_CHECK=n PMEM2_INSTALL=y
	fi
}

build_pmdk_package
