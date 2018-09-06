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
# src/test/libpmempool_feature/common.sh -- common part of libpmempool_feature tests
#

LOG=grep${UNITTEST_NUM}.log

# enum pmempool_feature values
PMEMPOOL_FEAT_SINGLEHDR=SINGLEHDR
PMEMPOOL_FEAT_CHCKSUM_2K=CKSUM_2K
PMEMPOOL_FEAT_SHUTDOWN_STATE=SHUTDOWN_STATE
PMEMPOOL_FEAT_INVALID=INVALID

FEATURE_PATTERN=feature.c
INFO_PATTERN="Mandatory features"

# grep_libpmempool_log -- grep libpmempool log if it exists, it does exists
# when build type is debug
function grep_libpmempool_log() {
	if [ -f "$PMEMPOOL_LOG_FILE" ]; then
		cat $PMEMPOOL_LOG_FILE | grep "$FEATURE_PATTERN" >> $LOG
	fi
}

# libpmempool_feature_test -- enable/ disablefeature
#
#	libpmempool_feature_test <enum-pmempool_feature-value> [UNSUPPORTED]
function libpmempool_feature_test() {
	case "$2" in
	UNSUPPORTED)
		exit_func=expect_abnormal_exit
		;;
	*)
		exit_func=expect_normal_exit
		;;
	esac

	# create pool
	expect_normal_exit $PMEMPOOL$EXESUFFIX create obj $DIR/pool.obj

	# enable feature
	$exit_func ./libpmempool_feature$EXESUFFIX $DIR/pool.obj e $1
	grep_libpmempool_log

	# query feature
	expect_normal_exit ./libpmempool_feature$EXESUFFIX $DIR/pool.obj q $1 >> $LOG

	# verify query by pmempool info
	expect_normal_exit $PMEMPOOL$EXESUFFIX info $DIR/pool.obj \
		| grep "$INFO_PATTERN" >> $LOG

	# check if pool is still valid
	expect_normal_exit $PMEMPOOL$EXESUFFIX check $DIR/pool.obj >> /dev/null

	# disable feature
	$exit_func ./libpmempool_feature$EXESUFFIX $DIR/pool.obj d $1
	grep_libpmempool_log

	# query feature
	expect_normal_exit ./libpmempool_feature$EXESUFFIX $DIR/pool.obj q $1 >> $LOG

	# verify query by pmempool info
	expect_normal_exit $PMEMPOOL$EXESUFFIX info $DIR/pool.obj\
		| grep "$INFO_PATTERN" >> $LOG

	# check if pool is still valid
	expect_normal_exit $PMEMPOOL$EXESUFFIX check $DIR/pool.obj >> /dev/null
}
