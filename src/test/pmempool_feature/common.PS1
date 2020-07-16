# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation

#
# src/test/pmempool_feature/common.ps1 -- common part of pmempool_feature tests
#

$PART_SIZE = (convert_to_bytes "10M")
$PART_SIZE_STR = ${PART_SIZE}.toString() + "B" #10MiB
$POOLSET="$DIR\testset"

# create poolset
create_poolset $POOLSET `
	${PART_SIZE_STR}:$DIR\testfile11:x ${PART_SIZE_STR}:$DIR\testfile12:x `
	R ${PART_SIZE_STR}:$DIR\testfile21:x ${PART_SIZE_STR}:$DIR\testfile22:x `
	R ${PART_SIZE_STR}:$DIR\testfile31:x

$LOG = "grep${Env:UNITTEST_NUM}.log"
remove_files $LOG

$exit_func="expect_normal_exit"

# pmempool_feature_query_return -- query a feature and return
# the value.
#
# usage: pmempool_feature_query_return <feature>
function pmempool_feature_query_return($arg1) {
	$count=$(expect_normal_exit $PMEMPOOL feature -q $arg1 $POOLSET)
	return $count
}

# pmempool_feature_query -- query feature
#
# usage: pmempool_feature_query <feature>
function pmempool_feature_query($arg1) {
	$val=$(expect_normal_exit $PMEMPOOL feature -q $arg1 $POOLSET)
	echo "query $arg1 result is $val" >> $LOG
}

# pmempool_feature_enable -- enable feature
#
# usage: pmempool_feature_enable <feature> [no-query]
function pmempool_feature_enable($arg1, $arg2) {
	& $exit_func $PMEMPOOL feature -e $arg1 $POOLSET 2>&1 | Select-String "$arg1" | %{$_.Line} >> $LOG
	if ( "x$arg2" -ne "xno-query" ){
		pmempool_feature_query $arg1
	}
}

# pmempool_feature_disable -- disable feature
#
# usage: pmempool_feature_disable <feature> [no-query]
function pmempool_feature_disable($arg1, $arg2) {
	& $exit_func $PMEMPOOL feature -d $arg1 $POOLSET 2>&1 | Select-String "$arg1" | %{$_.Line} >> $LOG
	if ( "x$arg2" -ne "xno-query" ){
		pmempool_feature_query $arg1
	}
}
