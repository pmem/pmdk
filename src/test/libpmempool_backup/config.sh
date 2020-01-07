#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017, Intel Corporation
#
#
# libpmempool_backup/config.sh -- test configuration
#

# Extend timeout for TEST0, as it may take more than a minute
# when run on a non-pmem file system.

CONF_TIMEOUT[0]='10m'

