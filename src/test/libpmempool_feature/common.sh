#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2024, Intel Corporation

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

# XXX disable log file verification until #5981 is resolved
# https://github.com/pmem/pmdk/issues/5981
if false; then
	if [ -f "$PMEMPOOL_LOG_FILE" ]; then
		cat $PMEMPOOL_LOG_FILE | grep "$ERROR_PATTERN" >> $LOG
	fi
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

# XXX disable log file verification until #5981 is resolved
# https://github.com/pmem/pmdk/issues/5981
if false; then
		if [ -f "$PMEMPOOL_LOG_FILE" ]; then
			cat $PMEMPOOL_LOG_FILE | grep "$ERROR_PATTERN" >> $LOG
		fi
fi
	fi
	if [ "x$2" != "xno-query" ]; then
		libpmempool_feature_query $1
	fi
	echo normal end
}

# libpmempool_feature_disable -- disable feature
#
# usage: libpmempool_feature_disable <enum-pmempool_feature> [no-query]
function libpmempool_feature_disable() {
	$exit_func ./libpmempool_feature$EXESUFFIX $POOL d $1
	if [ "$exit_func" == "expect_abnormal_exit" ]; then
# XXX disable log file verification until #5981 is resolved
# https://github.com/pmem/pmdk/issues/5981
if false; then
		if [ -f "$PMEMPOOL_LOG_FILE" ]; then
			cat $PMEMPOOL_LOG_FILE | grep "$ERROR_PATTERN" >> $LOG
		fi
fi
	fi
	if [ "x$2" != "xno-query" ]; then
		libpmempool_feature_query $1
	fi
}
