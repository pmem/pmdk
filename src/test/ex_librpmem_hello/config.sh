#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation
#
#
# ex_librpmem_hello/config.sh -- test configuration
#

# Filesystem-DAX cannot be used for RDMA
# since it is missing support in Linux kernel
CONF_GLOBAL_FS_TYPE=non-pmem

CONF_GLOBAL_BUILD_TYPE="debug nondebug"
CONF_GLOBAL_TEST_TYPE=short

CONF_GLOBAL_RPMEM_PROVIDER=all
CONF_GLOBAL_RPMEM_PMETHOD=all
