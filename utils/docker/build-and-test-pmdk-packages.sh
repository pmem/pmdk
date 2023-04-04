#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation

# build-and-test-pmdk-packages.sh - is called inside a Docker container; Script for building and testing pmdk packages.

set -eo pipefail

# Prepare build environment
# Adding 'PKG' parameter is required for PKG tests.
./prepare-for-build.sh PKG

# build_pmdk_package -- build pmdk package depending on the Linux distribution.
function build_pmdk_package {
	cd $WORKDIR
	echo "********** make package **********"
	date
	if [ $OS = opensuse/leap ] || [ $OS = rockylinux/rockylinux ]; then
		make rpm EXPERIMENTAL=y BUILD_PACKAGE_CHECK=n PMEM2_INSTALL=y
	elif [ $OS = ubuntu ]; then
		make dpkg EXPERIMENTAL=y BUILD_PACKAGE_CHECK=n PMEM2_INSTALL=y
	fi
}

# Build pmdk to create src/nondebug directory used by python scripts in tests.
function build_pmdk {
	cd $WORKDIR
	echo "********** make pmdk **********"
	make EXTRA_CFLAGS=-DUSE_VALGRIND
}

# test_built_packages -- test if the packages are built correctly.
function test_built_packages {
	echo "********** test built packages **********"
	python3 $SCRIPTSDIR/packages/test-build-packages.py -r $(pwd)
}

# install_packages -- install built packages.
function install_packages {
	echo "********** install packages **********"
	sudo python3 $SCRIPTSDIR/packages/install-packages.py -r $(pwd)
}

# test_installed_packages -- test if the packages were installed correctly.
function test_installed_packages {
	echo "********** test installed packages **********"
	python3 $SCRIPTSDIR/packages/test-packages-installation.py -r $(pwd)
}

function build_unittests {
	echo "********** build unittests **********"
	cd $WORKDIR/src/test && make clobber
	make test
}

function run_check_unittests {
	echo "********** run tests **********"
	cd $WORKDIR/src/test && ./RUNTESTS -t check -o 60m
}

build_pmdk_package
build_pmdk
test_built_packages
install_packages
test_installed_packages
build_unittests
run_check_unittests
