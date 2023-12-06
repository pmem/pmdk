#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022-2023, Intel Corporation

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
	cd ${PMDK_PATH} && make -j$(nproc) clean
	cd ${PMDK_PATH} && make -j$(nproc)
	echo "********** make pmdk test **********"
	cd ${PMDK_PATH}/ && make -j$(nproc) test
}

build_pmdk
