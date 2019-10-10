#!/usr/bin/env bash
#
# Copyright 2018-2019, Intel Corporation
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
# src/test/pmempool_feature/common.sh -- common part of pmempool_feature tests
#
# for feature values please see: pmempool feature help

PART_SIZE=$(convert_to_bytes 10M)

# define files and directories
POOLSET=$DIR/testset

TEST_SET_LOCAL=testset_local
TEST_SET_REMOTE=testset_remote

LOG=grep${UNITTEST_NUM}.log

pmempool_exe=$PMEMPOOL$EXESUFFIX
exit_func=expect_normal_exit
sds_enabled=$(is_ndctl_enabled $pmempool_exe; echo $?)

# pmempool_feature_query -- query feature
#
# usage: pmempool_feature_query <feature> [<query-exit-type>]
function pmempool_feature_query() {
	query_exit_type=${2-normal}
	query_exit_func=expect_${query_exit_type}_exit
	val=$($query_exit_func $pmempool_exe feature -q $1 $POOLSET 2>> $LOG)
	if [ "$query_exit_type" == "normal" ]; then
		echo "query $1 result is $val" &>> $LOG
	fi
}

# pmempool_feature_enable -- enable feature
#
# usage: pmempool_feature_enable <feature> [no-query]
function pmempool_feature_enable() {
	$exit_func $pmempool_exe feature -e $1 $POOLSET &>> $LOG
	if [ "x$2" != "xno-query" ]; then
		pmempool_feature_query $1
	fi
}

# pmempool_feature_disable -- disable feature
#
# usage: pmempool_feature_disable <feature> [no-query]
function pmempool_feature_disable() {
	$exit_func $pmempool_exe feature -d $1 $POOLSET '&>>' $LOG
	if [ "x$2" != "xno-query" ]; then
		pmempool_feature_query $1
	fi
}

# pmempool_feature_create_poolset -- create poolset
#
# usage: pmempool_feature_create_poolset <poolset-type>
function pmempool_feature_create_poolset() {
	POOLSET_TYPE=$1
	case "$1" in
	"no_dax_device")
		create_poolset $POOLSET \
			$PART_SIZE:$DIR/testfile11:x \
			$PART_SIZE:$DIR/testfile12:x \
			r \
			$PART_SIZE:$DIR/testfile21:x \
			$PART_SIZE:$DIR/testfile22:x \
			r \
			$PART_SIZE:$DIR/testfile31:x
			;;
	"dax_device")
		create_poolset $POOLSET \
			AUTO:$DEVICE_DAX_PATH
			;;
	"remote")
		create_poolset $DIR/$TEST_SET_LOCAL \
			$PART_SIZE:${NODE_DIR[1]}/testfile_local11:x \
			$PART_SIZE:${NODE_DIR[1]}/testfile_local12:x \
			m ${NODE_ADDR[0]}:$TEST_SET_REMOTE
		create_poolset $DIR/$TEST_SET_REMOTE \
			$PART_SIZE:${NODE_DIR[0]}/testfile_remote21:x \
			$PART_SIZE:${NODE_DIR[0]}/testfile_remote22:x

		copy_files_to_node 0 ${NODE_DIR[0]} $DIR/$TEST_SET_REMOTE
		copy_files_to_node 1 ${NODE_DIR[1]} $DIR/$TEST_SET_LOCAL

		rm_files_from_node 1 \
			${NODE_DIR[1]}testfile_local11 ${NODE_DIR[1]}testfile_local12
		rm_files_from_node 0 \
			${NODE_DIR[0]}testfile_remote21 ${NODE_DIR[0]}testfile_remote22

		POOLSET="${NODE_DIR[1]}/$TEST_SET_LOCAL"
		;;
	esac

	expect_normal_exit $pmempool_exe rm -f $POOLSET

	# create pool
	# pmempool create under valgrind pmemcheck takes too long
	# it is not part of the test so it is run here without valgrind
	VALGRIND_DISABLED=y expect_normal_exit $pmempool_exe create obj $POOLSET
}

# pmempool_feature_test_SINGLEHDR -- test SINGLEHDR
function pmempool_feature_test_SINGLEHDR() {
	exit_func=expect_abnormal_exit
	pmempool_feature_enable "SINGLEHDR" "no-query" # UNSUPPORTED
	pmempool_feature_disable "SINGLEHDR" "no-query" # UNSUPPORTED
	exit_func=expect_normal_exit
	pmempool_feature_query "SINGLEHDR"
}

# pmempool_feature_test_CKSUM_2K -- test CKSUM_2K
function pmempool_feature_test_CKSUM_2K() {
	# PMEMPOOL_FEAT_CHCKSUM_2K is enabled by default
	pmempool_feature_query "CKSUM_2K"

	# SHUTDOWN_STATE is disabled on Linux if PMDK is compiled with old ndctl
	# enable it to interfere toggling CKSUM_2K
	if [ $sds_enabled -eq 1 ]; then
		pmempool_feature_enable SHUTDOWN_STATE "no-query"
	fi

	# disable PMEMPOOL_FEAT_SHUTDOWN_STATE prior to success
	exit_func=expect_abnormal_exit
	pmempool_feature_disable "CKSUM_2K" # should fail
	exit_func=expect_normal_exit
	pmempool_feature_disable "SHUTDOWN_STATE"
	pmempool_feature_disable "CKSUM_2K" # should succeed

	pmempool_feature_enable "CKSUM_2K"
}

# pmempool_feature_test_SHUTDOWN_STATE -- test SHUTDOWN_STATE
function pmempool_feature_test_SHUTDOWN_STATE() {
	pmempool_feature_query "SHUTDOWN_STATE"

	if [ $sds_enabled -eq 0 ]; then
		pmempool_feature_disable SHUTDOWN_STATE
	fi

	# PMEMPOOL_FEAT_SHUTDOWN_STATE requires PMEMPOOL_FEAT_CHCKSUM_2K
	pmempool_feature_disable "CKSUM_2K"
	exit_func=expect_abnormal_exit
	pmempool_feature_enable "SHUTDOWN_STATE" # should fail
	exit_func=expect_normal_exit
	pmempool_feature_enable "CKSUM_2K"
	pmempool_feature_enable "SHUTDOWN_STATE" # should succeed
}

# pmempool_feature_test_CHECK_BAD_BLOCKS -- test SHUTDOWN_STATE
function pmempool_feature_test_CHECK_BAD_BLOCKS() {

	# PMEMPOOL_FEAT_CHECK_BAD_BLOCKS is disabled by default
	pmempool_feature_query "CHECK_BAD_BLOCKS"

	pmempool_feature_enable "CHECK_BAD_BLOCKS"
	pmempool_feature_disable "CHECK_BAD_BLOCKS"
}

# pmempool_feature_remote_init -- initialization remote replics
function pmempool_feature_remote_init() {
	require_nodes 2

	require_node_libfabric 0 $RPMEM_PROVIDER
	require_node_libfabric 1 $RPMEM_PROVIDER

	init_rpmem_on_node 1 0

	pmempool_exe="run_on_node 1 ../pmempool"
}

# pmempool_feature_test_remote -- run remote tests
function pmempool_feature_test_remote() {
	# create pool
	expect_normal_exit $pmempool_exe rm -f $POOLSET
	expect_normal_exit $pmempool_exe create obj $POOLSET

	# poolset with remote replicas are not supported
	exit_func=expect_abnormal_exit
	pmempool_feature_enable $1 no-query
	pmempool_feature_disable $1 no-query
	pmempool_feature_query $1 abnormal
}
