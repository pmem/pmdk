#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2017, Intel Corporation

#
# src/test/rpmem_basic/setup.sh -- common part for TEST* scripts
#
set -e

require_nodes 2
require_node_libfabric 0 $RPMEM_PROVIDER $SETUP_LIBFABRIC_VERSION
require_node_libfabric 1 $RPMEM_PROVIDER $SETUP_LIBFABRIC_VERSION
require_node_log_files 0 $RPMEMD_LOG_FILE
require_node_log_files 1 $RPMEM_LOG_FILE
require_node_log_files 1 $PMEM_LOG_FILE

POOLS_DIR=pools
POOLS_PART=pool_parts
PART_DIR=${NODE_DIR[0]}/$POOLS_PART
RPMEM_POOLSET_DIR[0]=${NODE_DIR[0]}$POOLS_DIR

if [ -z "$SETUP_MANUAL_INIT_RPMEM" ]; then
	init_rpmem_on_node 1 0
fi
