#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2021, Intel Corporation

#
# buildPmdk.sh - Script for building pmdk packages
#

echo "make pmdk"

FULL_PATH=$(readlink -f .)
PMDK_0_PATH=$(dirname $FULL_PATH)
PMDK_PATH=${PMDK_0_PATH}/pmdk
echo "Fullpath:" ${PMDK_PATH}

source ${PMDK_PATH}/utils/gha-runners/common.sh

build_pmdk --pmdk-path=${PMDK_PATH}