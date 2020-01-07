#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2018, Intel Corporation

#
# src/test/rpmem_basic/common_pm_policy.sh -- common part for TEST[10-11] scripts
#
set -e

LOG=rpmemd$UNITTEST_NUM.log
OUT=out$UNITTEST_NUM.log
rm -f $OUT

# create poolset and upload
run_on_node 0 "rm -rf ${RPMEM_POOLSET_DIR[0]} && mkdir -p ${RPMEM_POOLSET_DIR[0]} && mkdir -p ${NODE_DIR[0]}$POOLS_PART"
create_poolset $DIR/pool.set 8M:$PART_DIR/pool.part0 8M:$PART_DIR/pool.part1
copy_files_to_node 0 ${RPMEM_POOLSET_DIR[0]} $DIR/pool.set

# create pool and close it - local pool from file
SIMPLE_ARGS="test_create 0 pool.set ${NODE_ADDR[0]} pool 8M none test_close 0"

function test_pm_policy()
{
	# initialize pmem
	PMEM_IS_PMEM_FORCE=$1
	export_vars_node 0 PMEM_IS_PMEM_FORCE
	init_rpmem_on_node 1 0

	# remove rpmemd log and pool parts
	run_on_node 0 "rm -rf $PART_DIR && mkdir -p $PART_DIR && rm -f $LOG"

	# execute, get log
	expect_normal_exit run_on_node 1 ./rpmem_basic$EXESUFFIX $SIMPLE_ARGS
	copy_files_from_node 0 . ${NODE_TEST_DIR[0]}/$LOG

	# extract persist method and flush function
	cat $LOG | $GREP -A2 "persistency policy:" >> $OUT
}
