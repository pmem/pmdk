#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017, Intel Corporation
#
#
# pmempool_check/config.sh -- test configuration
#

# Extend timeout for TEST5, as it may take a few minutes
# when run on a non-pmem file system.

CONF_TIMEOUT[5]='10m'

