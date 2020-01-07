#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2017, Intel Corporation

#
# src/test/rpmem_basic/setup2to1.sh -- common part for TEST[7-] scripts
#

POOLS_DIR=pools
POOLS_PART=pool_parts

TEST_LOG_FILE=test$UNITTEST_NUM.log
TEST_LOG_LEVEL=3

#
# This unit test requires 4 nodes, but nodes #0 and #3 should be the same
# physical machine - they should have the same NODE[n] addresses but
# different NODE_ADDR[n] addresses in order to test "2-to-1" configuration.
# Node #1 is being replicated to the node #0 and node #2 is being replicated
# to the node #3.
#
require_nodes 4

REPLICA[1]=0
REPLICA[2]=3

for node in 1 2; do
	require_node_libfabric ${node}             $RPMEM_PROVIDER
	require_node_libfabric ${REPLICA[${node}]} $RPMEM_PROVIDER

	export_vars_node       ${node}		   TEST_LOG_FILE
	export_vars_node       ${node}             TEST_LOG_LEVEL

	require_node_log_files ${node}             $PMEM_LOG_FILE
	require_node_log_files ${node}             $RPMEM_LOG_FILE
	require_node_log_files ${node}             $TEST_LOG_FILE
	require_node_log_files ${REPLICA[${node}]} $RPMEMD_LOG_FILE

	REP_ADDR[${node}]=${NODE_ADDR[${REPLICA[${node}]}]}

	PART_DIR[${node}]=${NODE_DIR[${REPLICA[${node}]}]}$POOLS_PART
	RPMEM_POOLSET_DIR[${REPLICA[${node}]}]=${NODE_DIR[${REPLICA[${node}]}]}$POOLS_DIR

	init_rpmem_on_node ${node} ${REPLICA[${node}]}

	PID_FILE[${node}]="pidfile${UNITTEST_NUM}-${node}.pid"
	clean_remote_node ${node} ${PID_FILE[${node}]}
done
