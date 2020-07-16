# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation

#
# src/test/libpmempool_feature/common.ps1 -- common part of libpmempool_feature tests
#

$POOL = "$DIR\pool.obj"

$OUT = "out${Env:UNITTEST_NUM}.log"
$LOG = "grep${Env:UNITTEST_NUM}.log"
remove_files $LOG

$QUERY_PATTERN="query"
$ERROR_PATTERN="<1> \[feature.c:.*\]"

$exit_func="expect_normal_exit"

# pmempool_feature_query_return -- query a feature and return
# the value.
#
# usage: pmempool_feature_query_return <feature>
function pmempool_feature_query_return($arg1) {
	# verify query by pmempool info
	$count=(expect_normal_exit $PMEMPOOL info $POOL | Select-String "$arg1").length
	return $count
}

# libpmempool_feature_query_abnormal -- query feature with expected
#	abnormal result
#
# usage: libpmempool_feature_query_abnormal <enum-pmempool_feature>
function libpmempool_feature_query_abnormal($arg1) {
	# query feature
	expect_abnormal_exit $Env:EXE_DIR\libpmempool_feature$Env:EXESUFFIX $POOL q $arg1
	cat $OUT | Select-String "$QUERY_PATTERN" | %{$_.Line} >> $LOG
	if ( Test-Path $Env:PMEMPOOL_LOG_FILE ) {
		cat $Env:PMEMPOOL_LOG_FILE | Select-String "$ERROR_PATTERN" | %{$_.Line} >> $LOG
	}
}

# libpmempool_feature_query -- query feature
#
# usage: libpmempool_feature_query <enum-pmempool_feature>
function libpmempool_feature_query($arg1) {
	# query feature
	expect_normal_exit $Env:EXE_DIR\libpmempool_feature$Env:EXESUFFIX $POOL q $arg1
	cat $OUT | Select-String "$QUERY_PATTERN" | %{$_.Line} >> $LOG

	# verify query by pmempool info
	$count=(expect_normal_exit $PMEMPOOL info $POOL | Select-String "$arg1").length
	if ( "$count" -eq "0" ){
		echo "pmempool info: $arg1 is NOT set" >> $LOG
	}else{
		echo "pmempool info: $arg1 is set" >> $LOG
	}

	# check if pool is still valid
	expect_normal_exit $PMEMPOOL check $POOL | out-null
}

# libpmempool_feature_enable -- enable feature
#
# usage: libpmempool_feature_enable <enum-pmempool_feature> [no-query]
function libpmempool_feature_enable($arg1, $arg2) {
	& $exit_func $Env:EXE_DIR\libpmempool_feature$Env:EXESUFFIX $POOL e $arg1 2>&1 `
		| Select-String "$arg1" | %{$_.Line} >> $LOG
	if ( "$exit_func" -eq "expect_abnormal_exit" ) {
		if ( Test-Path $Env:PMEMPOOL_LOG_FILE ) {
			cat $Env:PMEMPOOL_LOG_FILE | Select-String "$ERROR_PATTERN" | %{$_.Line} >> $LOG
		}
	}
	if ( "$arg2" -ne "no-query" ){
		libpmempool_feature_query $arg1
	}
}

# libpmempool_feature_disable -- disable feature
#
# usage: libpmempool_feature_disable <enum-pmempool_feature> [no-query]
function libpmempool_feature_disable($arg1, $arg2) {
	& $exit_func $Env:EXE_DIR\libpmempool_feature$Env:EXESUFFIX $POOL d $arg1 2>&1 `
		| Select-String "$arg1" | %{$_.Line} >> $LOG
	if ( "$exit_func" -eq "expect_abnormal_exit" ) {
		cat $Env:PMEMPOOL_LOG_FILE | Select-String "$ERROR_PATTERN" | %{$_.Line} >> $LOG
	}
	if ( "$arg2" -ne "no-query" ){
		libpmempool_feature_query $arg1
	}
}
