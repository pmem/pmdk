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
# src/test/libpmempool_feature/common.sh -- common part of libpmempool_feature tests
#

POOL=$DIR/pool.obj

OUT=out${UNITTEST_NUM}.log
LOG=grep${UNITTEST_NUM}.log

QUERY_PATTERN="query"
ERROR_PATTERN="<1> \\[feature.c:.*\\]"

exit_func=expect_normal_exit
sds_enabled=$(is_ndctl_enabled ./libpmempool_feature$EXESUFFIX; echo $?)

# libpmempool_feature_query_abnormal -- query feature with expected
#	abnormal result
#
# usage: libpmempool_feature_query_abnormal <enum-pmempool_feature>
function libpmempool_feature_query_abnormal() {
	# query feature
	expect_abnormal_exit ./libpmempool_feature$EXESUFFIX $POOL q $1
	if [ -f "$PMEMPOOL_LOG_FILE" ]; then
		cat $PMEMPOOL_LOG_FILE | grep "$ERROR_PATTERN" >> $LOG
	fi
}

# libpmempool_feature_query -- query feature
#
# usage: libpmempool_feature_query <enum-pmempool_feature>
function libpmempool_feature_query() {
	# query feature
	expect_normal_exit ./libpmempool_feature$EXESUFFIX $POOL q $1
	cat $OUT | grep "$QUERY_PATTERN" >> $LOG

	# verify query with pmempool info
	set +e
	count=$(expect_normal_exit $PMEMPOOL$EXESUFFIX info $POOL | grep -c "$1")
	set -e
	if [ "$count" = "0" ]; then
		echo "pmempool info: $1 is NOT set" >> $LOG
	else
		echo "pmempool info: $1 is set" >> $LOG
	fi

	# check if pool is still valid
	expect_normal_exit $PMEMPOOL$EXESUFFIX check $POOL >> /dev/null
}

# libpmempool_feature_enable -- enable feature
#
# usage: libpmempool_feature_enable <enum-pmempool_feature> [no-query]
function libpmempool_feature_enable() {
	$exit_func ./libpmempool_feature$EXESUFFIX $POOL e $1
	if [ "$exit_func" == "expect_abnormal_exit" ]; then
		if [ -f "$PMEMPOOL_LOG_FILE" ]; then
			cat $PMEMPOOL_LOG_FILE | grep "$ERROR_PATTERN" >> $LOG
		fi
	fi
	if [ "x$2" != "xno-query" ]; then
		libpmempool_feature_query $1
	fi
}

# libpmempool_feature_disable -- disable feature
#
# usage: libpmempool_feature_disable <enum-pmempool_feature> [no-query]
function libpmempool_feature_disable() {
	$exit_func ./libpmempool_feature$EXESUFFIX $POOL d $1
	if [ "$exit_func" == "expect_abnormal_exit" ]; then
		if [ -f "$PMEMPOOL_LOG_FILE" ]; then
			cat $PMEMPOOL_LOG_FILE | grep "$ERROR_PATTERN" >> $LOG
		fi
	fi
	if [ "x$2" != "xno-query" ]; then
		libpmempool_feature_query $1
	fi
}
