#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation
#
#
# pmempool_feature_remote/config.sh -- test configuration
#

CONF_GLOBAL_FS_TYPE=any
CONF_GLOBAL_BUILD_TYPE="debug nondebug"

# pmempool feature does not support poolsets with remote replicas
# unittest contains only negative scenarios so no point to loop over
# all providers and persistency methods
CONF_GLOBAL_RPMEM_PROVIDER=sockets
CONF_GLOBAL_RPMEM_PMETHOD=GPSPM
