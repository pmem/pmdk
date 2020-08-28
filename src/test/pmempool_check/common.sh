#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation
#
#
# pmempool_check/common.sh -- checking pools helpers
#

LOG=out${UNITTEST_NUM}.log
rm -f $LOG && touch $LOG

LAYOUT=OBJ_LAYOUT$SUFFIX
POOLSET=$DIR/poolset

pmempool_exe=$PMEMPOOL$EXESUFFIX

# pmempool_feature_query_return -- query a feature and return
# the value.
#
# usage: pmempool_feature_query_return <feature>
function pmempool_feature_query_return() {
	return $($pmempool_exe feature -q $1 $POOLSET 2>> $LOG)
}

# pmemspoil_corrupt_replica_sds -- corrupt shutdown state
#
#	usage: pmemspoil_corrupt_replica_sds <replica>
function pmemspoil_corrupt_replica_sds() {
	local replica=$1
	expect_normal_exit $PMEMSPOIL --replica $replica $POOLSET \
		pool_hdr.shutdown_state.usc=999 \
		pool_hdr.shutdown_state.dirty=1 \
		"pool_hdr.shutdown_state.f:checksum_gen"
}

# pmempool_check_sds_init -- shutdown state unittest init
#
#	usage: pmempool_check_sds [enable-sds]
function pmempool_check_sds_init() {
	# initialize poolset
	create_poolset $POOLSET \
		8M:$DIR/part00:x \
		r 8M:$DIR/part10:x

	# enable SHUTDOWN_STATE feature
	if [ "x$1" == "xenable-sds" ]; then
		local conf="sds.at_create=1"
	fi

	PMEMOBJ_CONF="${PMEMOBJ_CONF}$conf;"
	expect_normal_exit $PMEMPOOL$EXESUFFIX create --layout=$LAYOUT obj $POOLSET

	# If SDS is not enabled at this point is because SDS is not available for
	# this device
	pmempool_feature_query_return "SHUTDOWN_STATE"
	if [[ $? -eq 0 ]]; then
		msg "$UNITTEST_NAME: SKIP: SDS is not available"
		exit 0
	fi

}

# pmempool_check_sds -- perform shutdown state unittest
#
#	usage: pmempool_check_sds <scenario>
function pmempool_check_sds() {
	# corrupt poolset replicas
	pmemspoil_corrupt_replica_sds 0
	pmemspoil_corrupt_replica_sds 1

	# verify it is corrupted
	expect_abnormal_exit $PMEMPOOL$EXESUFFIX check $POOLSET >> $LOG 2>/dev/null
	exit_func=expect_normal_exit

	# perform fixes
	case "$1" in
	fix_second_replica_only)
		echo -e "n\ny\n" | expect_normal_exit $PMEMPOOL$EXESUFFIX check -vr $POOLSET >> $LOG 2>/dev/null
		;;
	fix_first_replica)
		echo -e "y\n" | expect_normal_exit $PMEMPOOL$EXESUFFIX check -vr $POOLSET >> $LOG 2>/dev/null
		;;
	fix_no_replicas)
		echo -e "n\nn\n" | expect_abnormal_exit $PMEMPOOL$EXESUFFIX check -vr $POOLSET >> $LOG 2>/dev/null
		exit_func=expect_abnormal_exit
		;;
	*)
		fatal "unittest_sds: undefined scenario '$1'"
		;;
	esac

	#verify result
	$exit_func $PMEMPOOL$EXESUFFIX check $POOLSET >> $LOG 2>/dev/null
}
