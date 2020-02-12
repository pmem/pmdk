#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/rpmem_obc/setup.sh -- common setup for rpmem_obc tests
#
set -e

require_nodes 2
require_node_log_files 1 $RPMEM_LOG_FILE

RPMEM_CMD="\"cd ${NODE_TEST_DIR[0]} && UNITTEST_FORCE_QUIET=1 \
	LD_LIBRARY_PATH=${NODE_LD_LIBRARY_PATH[0]}:$REMOTE_LD_LIBRARY_PATH \
	./rpmem_obc$EXESUFFIX\""

export_vars_node 1 RPMEM_CMD
