#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/rpmem_fip/setup.sh -- common setup for rpmem_fip tests
#
set -e

require_nodes 2
require_node_libfabric 0 $RPMEM_PROVIDER
require_node_libfabric 1 $RPMEM_PROVIDER
require_node_log_files 0 $RPMEM_LOG_FILE $RPMEMD_LOG_FILE
require_node_log_files 1 $RPMEM_LOG_FILE $RPMEMD_LOG_FILE

SRV=srv${UNITTEST_NUM}.pid
clean_remote_node 0 $SRV
RPMEM_CMD="\"cd ${NODE_TEST_DIR[0]} && RPMEMD_LOG_LEVEL=\$RPMEMD_LOG_LEVEL RPMEMD_LOG_FILE=\$RPMEMD_LOG_FILE UNITTEST_FORCE_QUIET=1 \
	LD_LIBRARY_PATH=${NODE_LD_LIBRARY_PATH[0]}:$REMOTE_LD_LIBRARY_PATH \
	./rpmem_fip$EXESUFFIX\""

export_vars_node 1 RPMEM_CMD

if [ -n ${RPMEM_MAX_NLANES+x} ]; then
	export_vars_node 1 RPMEM_MAX_NLANES
fi
