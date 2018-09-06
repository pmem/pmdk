#!/usr/bin/env bash
#
# Copyright 2018, Intel Corporation
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

LOG=grep${UNITTEST_NUM}.log

exit_func=expect_normal_exit

# pmempool_feature_query -- query feature
#
# usage: pmempool_feature_query <feature>
function pmempool_feature_query() {
	val=$(expect_normal_exit $PMEMPOOL$EXESUFFIX feature -q $1 $DIR/pool.obj)
	echo "query $1 result is $val" &>> $LOG
}

# pmempool_feature_enable -- enable feature
#
# usage: pmempool_feature_enable <feature>
function pmempool_feature_enable() {
	$exit_func $PMEMPOOL$EXESUFFIX feature -e $1 $DIR/pool.obj &>> $LOG
	if [ "x$2" != "xno-query" ]; then
		pmempool_feature_query $1
	fi
}

# pmempool_feature_disable -- disable feature
#
# usage: pmempool_feature_disable <feature>
function pmempool_feature_disable() {
	$exit_func $PMEMPOOL$EXESUFFIX feature -d $1 $DIR/pool.obj &>> $LOG
	if [ "x$2" != "xno-query" ]; then
		pmempool_feature_query $1
	fi
}

# pmempool_feature_test -- misc scenarios for each feature value
function pmempool_feature_test() {
	# create pool
	expect_normal_exit $PMEMPOOL$EXESUFFIX create obj $DIR/pool.obj

	case "$1" in
	"SINGLEHDR")
		exit_func=expect_abnormal_exit
		pmempool_feature_enable $1 no-query # UNSUPPORTED
		pmempool_feature_disable $1 no-query # UNSUPPORTED
		pmempool_feature_query $1
		;;
	"CKSUM_2K")
		# PMEMPOOL_FEAT_CHCKSUM_2K is enabled by default
		pmempool_feature_query $1

		# disable PMEMPOOL_FEAT_SHUTDOWN_STATE prior to success
		exit_func=expect_abnormal_exit
		pmempool_feature_disable $1 # should fail
		exit_func=expect_normal_exit
		pmempool_feature_disable "SHUTDOWN_STATE"
		pmempool_feature_disable $1 # should succeed

		pmempool_feature_enable $1
		;;
	"SHUTDOWN_STATE")
		# PMEMPOOL_FEAT_SHUTDOWN_STATE is enabled by default
		pmempool_feature_query $1

		pmempool_feature_disable $1

		# PMEMPOOL_FEAT_SHUTDOWN_STATE requires PMEMPOOL_FEAT_CHCKSUM_2K
		pmempool_feature_disable "CKSUM_2K"
		exit_func=expect_abnormal_exit
		pmempool_feature_enable $1 # should fail
		exit_func=expect_normal_exit
		pmempool_feature_enable "CKSUM_2K"
		pmempool_feature_enable $1 # should succeed
		;;
	esac
}
