#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2018, Intel Corporation
#
#
# pmempool_transform_remote/common.sh -- commons for pmempool transform tests
#                                        with remote replication
#
set -e

require_nodes 2

require_node_libfabric 0 $RPMEM_PROVIDER
require_node_libfabric 1 $RPMEM_PROVIDER

setup

init_rpmem_on_node 1 0

require_node_log_files 1 pmemobj$UNITTEST_NUM.log
require_node_log_files 1 pmempool$UNITTEST_NUM.log

LOG=out${UNITTEST_NUM}.log
LOG_TEMP=out${UNITTEST_NUM}_part.log
rm -f $LOG && touch $LOG
rm -f $LOG_TEMP && touch $LOG_TEMP
rm_files_from_node 0 ${NODE_TEST_DIR[0]}/$LOG
rm_files_from_node 1 ${NODE_TEST_DIR[1]}/$LOG

LAYOUT=OBJ_LAYOUT
POOLSET_LOCAL_IN=poolset.in
POOLSET_LOCAL_OUT=poolset.out
POOLSET_REMOTE=poolset.remote
POOLSET_REMOTE1=poolset.remote1
POOLSET_REMOTE2=poolset.remote2

# CLI scripts for writing and reading some data hitting all the parts
WRITE_SCRIPT="pmemobjcli.write.script"
READ_SCRIPT="pmemobjcli.read.script"

copy_files_to_node 1 ${NODE_DIR[1]} $WRITE_SCRIPT $READ_SCRIPT

DUMP_INFO_LOG="../pmempool info"
DUMP_INFO_LOG_REMOTE="$DUMP_INFO_LOG -f obj"
DUMP_INFO_SED="sed -e '/^Checksum/d' -e '/^Creation/d' -e '/^Previous replica UUID/d' -e '/^Next replica UUID/d'"
DUMP_INFO_SED_REMOTE="$DUMP_INFO_SED -e '/^Previous part UUID/d' -e '/^Next part UUID/d'"

function dump_info_log() {
	local node=$1
	local poolset=$2
	local name=$3
	local ignore=$4

	local sed_cmd="$DUMP_INFO_SED"
	if [ -n "$ignore" ]; then
		sed_cmd="$sed_cmd -e '/^$ignore/d'"
	fi

	expect_normal_exit run_on_node $node "\"$DUMP_INFO_LOG $poolset | $sed_cmd >> $name\""
}

function dump_info_log_remote() {
	local node=$1
	local poolset=$2
	local name=$3
	local ignore=$4

	local sed_cmd="$DUMP_INFO_SED_REMOTE"
	if [ -n "$ignore" ]; then
		sed_cmd="$sed_cmd -e '/^$ignore/d'"
	fi

	expect_normal_exit run_on_node $node "\"$DUMP_INFO_LOG_REMOTE $poolset | $sed_cmd >> $name\""
}

function diff_log() {
	local node=$1
	local f1=$2
	local f2=$3

	expect_normal_exit run_on_node $node "\"[ -s $f1 ] && [ -s $f2 ] && diff $f1 $f2\""
}

exec_pmemobjcli_script() {
	local node=$1
	local script=$2
	local poolset=$3
	local out=$4

	expect_normal_exit run_on_node $node "\"../pmemobjcli -s $script $poolset > $out \""
}
