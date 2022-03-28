#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

#
# build-pmdk.sh - Script for building pmdk project
#

FULL_PATH=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
PMDK_PATH="${FULL_PATH}/../.."

set -eo pipefail

#
# build_pmdk -- build pmdk from source
#
function build_pmdk {
	echo "********** make pmdk **********"
	cd ${PMDK_PATH} && make clean
	cd ${PMDK_PATH} && make EXTRA_CFLAGS=-DUSE_VALGRIND -j$(nproc)
	echo "********** make pmdk test **********"
	cd ${PMDK_PATH}/ && make test
}

build_pmdk
