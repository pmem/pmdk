#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_sync_remote/common.sh -- pmempool sync with remote replication
#
set -e

require_nodes 2

require_node_libfabric 0 $RPMEM_PROVIDER
require_node_libfabric 1 $RPMEM_PROVIDER

setup

init_rpmem_on_node 1 0

require_node_log_files 1 pmemobj$UNITTEST_NUM.log
require_node_log_files 1 pmempool$UNITTEST_NUM.log

PMEMOBJCLI_SCRIPT="pmemobjcli.script"
copy_files_to_node 1 ${NODE_TEST_DIR[1]} $PMEMOBJCLI_SCRIPT

POOLSET_LOCAL="local_pool.set"

#
# configure_poolsets -- configure pool set files for test
# usage: configure_poolsets <local replicas> <remote replicas>
#
function configure_poolsets() {
	local n_local=$1
	local n_remote=$2
	local poolset_args="8M:${NODE_DIR[1]}/pool.part.1:x
		8M:${NODE_DIR[1]}/pool.part.2:x"

	for i in $(seq 0 $((n_local - 1))); do
		poolset_args="$poolset_args R 8M:${NODE_DIR[1]}/pool.$i.part.1:x
		8M:${NODE_DIR[1]}/pool.$i.part.2:x"
	done

	for i in $(seq 0 $((n_remote - 1))); do
		POOLSET_REMOTE[$i]="remote_pool.$i.set"

		create_poolset $DIR/${POOLSET_REMOTE[$i]}\
			8M:${NODE_DIR[0]}remote.$i.part.1:x\
			8M:${NODE_DIR[0]}remote.$i.part.2:x

		copy_files_to_node 0 ${NODE_DIR[0]} $DIR/${POOLSET_REMOTE[$i]}

		poolset_args="$poolset_args m ${NODE_ADDR[0]}:${POOLSET_REMOTE[$i]}"
	done

	create_poolset $DIR/$POOLSET_LOCAL $poolset_args
	copy_files_to_node 1 ${NODE_DIR[1]} $DIR/$POOLSET_LOCAL

	expect_normal_exit run_on_node 1 ../pmempool rm -sf ${NODE_DIR[1]}$POOLSET_LOCAL
	expect_normal_exit run_on_node 1 ../pmempool create obj ${NODE_DIR[1]}$POOLSET_LOCAL
	expect_normal_exit run_on_node 1 ../pmemobjcli -s $PMEMOBJCLI_SCRIPT ${NODE_DIR[1]}$POOLSET_LOCAL > /dev/null
}

DUMP_INFO_LOG="../pmempool info -lHZCOoAa"
DUMP_INFO_LOG_REMOTE="$DUMP_INFO_LOG -f obj"
DUMP_INFO_SED="sed -e '/^Checksum/d' -e '/^Creation/d'"
DUMP_INFO_SED_REMOTE="$DUMP_INFO_SED -e '/^Previous part UUID/d' -e '/^Next part UUID/d'"

function dump_info_log() {
	local node=$1
	local rep=$2
	local poolset=$3
	local name=$4
	local ignore=$5

	local sed_cmd="$DUMP_INFO_SED"
	if [ -n "$ignore" ]; then
		sed_cmd="$sed_cmd -e '/^$ignore/d'"
	fi

	expect_normal_exit run_on_node $node "\"$DUMP_INFO_LOG -p $rep $poolset | $sed_cmd > $name\""
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

	expect_normal_exit run_on_node $node "\"$DUMP_INFO_LOG_REMOTE $poolset | $sed_cmd > $name\""
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
