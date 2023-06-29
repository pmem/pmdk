#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2023, Intel Corporation
#
#
# libpmempool_backup/common.sh -- functions for libpmempool_backup unittest
#
set -e

POOLSET=$DIR/pool.set
BACKUP=_backup
REPLICA=_replica
POOL_PART=$DIR/pool.part

OUT=out${UNITTEST_NUM}.log
OUT_TEMP=out${UNITTEST_NUM}_temp.log
DIFF=diff${UNITTEST_NUM}.log
rm -f $LOG $DIFF $OUT_TEMP && touch $LOG $DIFF $OUT_TEMP

# params for log and obj pools
POOL_TYPES=( obj )
POOL_CREATE_PARAMS=( "--layout test_layout" )
POOL_CHECK_PARAMS=( "-soOaAbZH -l -C" )

# create_poolset_variation -- create one from the tested poolset variation
#    usage: create_poolset_variation <variation-id> [<suffix>]
#
function create_poolset_variation() {
	local sfx=""
	local variation=$1
	shift

	if [ $# -gt 0 ]; then
		sfx=$1
	fi

	case "$variation"
	in
	1)
		# valid poolset file
		create_poolset $POOLSET$sfx \
			20M:${POOL_PART}1$sfx:x \
			20M:${POOL_PART}2$sfx:x \
			20M:${POOL_PART}3$sfx:x \
			20M:${POOL_PART}4$sfx:x
		;;
	2)
		# valid poolset file with replica
		create_poolset $POOLSET$sfx \
			20M:${POOL_PART}1$sfx:x \
			20M:${POOL_PART}2$sfx:x \
			20M:${POOL_PART}3$sfx:x \
			20M:${POOL_PART}4$sfx:x \
			r 80M:${POOL_PART}${REPLICA}$sfx:x
		;;
	3)
		# other number of parts
		create_poolset $POOLSET$sfx \
			20M:${POOL_PART}1$sfx:x \
			20M:${POOL_PART}2$sfx:x \
			40M:${POOL_PART}3$sfx:x
		;;
	4)
		# no poolset
		# return without check_file
		return
		;;
	5)
		# empty
		create_poolset $POOLSET$sfx
		;;
	6)
		# other size of part
		create_poolset $POOLSET$sfx \
			20M:${POOL_PART}1$sfx:x \
			20M:${POOL_PART}2$sfx:x \
			20M:${POOL_PART}3$sfx:x \
			21M:${POOL_PART}4$sfx:x
		;;
	esac

	check_file $POOLSET$sfx
}

#
# backup_and_compare -- perform backup and compare backup result with original
# if compare parameters are provided
#    usage: backup_and_compare <poolset> <type> [<compare-params>]
#
function backup_and_compare () {
	local poolset=$1
	local type=$2
	shift 2

	# backup
	expect_normal_exit ../libpmempool_api/libpmempool_test$EXESUFFIX \
		-b $poolset$BACKUP -t $type -r 1 $poolset
	cat $OUT >> $OUT_TEMP

	# compare
	if [ $# -gt 0 ]; then
		compare_replicas "$1" $poolset $poolset$BACKUP >> $DIFF
	fi
}

ALL_POOL_PARTS="${POOL_PART}1 ${POOL_PART}2 ${POOL_PART}3 ${POOL_PART}4 \
	${POOL_PART}${REPLICA}"
ALL_POOL_BACKUP_PARTS="${POOL_PART}1$BACKUP ${POOL_PART}2$BACKUP \
	${POOL_PART}3$BACKUP ${POOL_PART}4$BACKUP \
	${POOL_PART}${BACKUP}${REPLICA}"

#
# backup_cleanup -- perform cleanup between test cases
#
function backup_cleanup() {
	rm -f $POOLSET$BACKUP $ALL_POOL_PARTS $ALL_POOL_BACKUP_PARTS
}
