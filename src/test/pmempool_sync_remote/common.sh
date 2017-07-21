#!/usr/bin/env bash
#
# Copyright 2016-2017, Intel Corporation
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
# pmempool_sync_remote/common.sh -- pmempool sync with remote replication
#
set -e

require_nodes 2

require_node_libfabric 0 $RPMEM_PROVIDER
require_node_libfabric 1 $RPMEM_PROVIDER

init_rpmem_on_node 1 0

require_node_log_files 1 pmemobj$UNITTEST_NUM.log
require_node_log_files 1 pmempool$UNITTEST_NUM.log

PMEMOBJCLI_SCRIPT="pmemobjcli.script"
copy_files_to_node 1 . $PMEMOBJCLI_SCRIPT

POOLSET_LOCAL="local_pool.set"
NODE_DIRS=($(get_node_dir 0) $(get_node_dir 1))

#
# configure_poolsets -- configure pool set files for test
# usage: configure_poolsets <local replicas> <remote replicas>
#
function configure_poolsets() {
	local n_local=$1
	local n_remote=$2
	local poolset_args="8M:${NODE_DIRS[1]}/pool.part.1:x
		8M:${NODE_DIRS[1]}/pool.part.2:x"

	for i in $(seq 0 $((n_local - 1))); do
		poolset_args="$poolset_args R 8M:${NODE_DIRS[1]}/pool.$i.part.1:x
		8M:${NODE_DIRS[1]}/pool.$i.part.2:x"
	done

	for i in $(seq 0 $((n_remote - 1))); do
		POOLSET_REMOTE[$i]="remote_pool.$i.set"

		create_poolset $DIR/${POOLSET_REMOTE[$i]}\
			8M:${NODE_DIRS[0]}/remote.$i.part.1:x\
			8M:${NODE_DIRS[0]}/remote.$i.part.2:x

		copy_files_to_node 0 . $DIR/${POOLSET_REMOTE[$i]}

		poolset_args="$poolset_args m ${NODE_ADDR[0]}:${POOLSET_REMOTE[$i]}"
	done

	create_poolset $DIR/$POOLSET_LOCAL $poolset_args
	copy_files_to_node 1 . $DIR/$POOLSET_LOCAL

	expect_normal_exit run_on_node 1 ../pmempool rm -sf $POOLSET_LOCAL
	expect_normal_exit run_on_node 1 ../pmempool create obj $POOLSET_LOCAL
	expect_normal_exit run_on_node 1 ../pmemobjcli -s $PMEMOBJCLI_SCRIPT $POOLSET_LOCAL > /dev/null
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
