#!/bin/bash -e
#
# Copyright 2017, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# src/test/rpmem_basic/common_pm_policy.sh -- common part for TEST[10-11] scripts
#

LOG=rpmemd$UNITTEST_NUM.log
OUT=out$UNITTEST_NUM.log
rm -f $OUT

# create poolset and upload
run_on_node 0 "rm -rf $POOLS_DIR && mkdir -p $POOLS_DIR"
create_poolset $DIR/pool.set  8M:$PART_DIR/pool.part0 8M:$PART_DIR/pool.part1
copy_files_to_node 0 $POOLS_DIR $DIR/pool.set

# create pool and close it - local pool from file
SIMPLE_ARGS="test_create 0 pool.set ${NODE_ADDR[0]} pool 8M test_close 0"

function test_pm_policy()
{
	# initialize pmem
	PMEM_IS_PMEM_FORCE=$1
	export_vars_node 0 PMEM_IS_PMEM_FORCE
	init_rpmem_on_node 1 0

	# remove rpmemd log and pool parts
	run_on_node 0 "rm -rf $POOLS_PART && mkdir -p $POOLS_PART && rm -f $LOG"

	# execute, get log
	expect_normal_exit run_on_node 1 ./rpmem_basic$EXESUFFIX $SIMPLE_ARGS
	copy_files_from_node 0 . $LOG

	# extract persist method and flush function
	cat $LOG | grep -A2 "persistency policy:" >> $OUT
}
