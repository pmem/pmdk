#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# obj_sync/config.sh -- test configuration
#

# Extend timeout for this test, as it may take a few minutes
# when run on a non-pmem file system.

CONF_GLOBAL_TIMEOUT='240m'

