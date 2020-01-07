#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017, Intel Corporation
#
#
# pmempool_info_remote/config.sh -- test configuration
#
set -e

CONF_GLOBAL_FS_TYPE=any
CONF_GLOBAL_BUILD_TYPE="debug nondebug"

CONF_GLOBAL_RPMEM_PROVIDER=sockets
CONF_GLOBAL_RPMEM_PMETHOD=GPSPM
