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
rm -rf $LOG $DIFF $OUT_TEMP && touch $LOG $DIFF $OUT_TEMP

# params for blk, log and obj pools
POOL_TYPES=( blk log obj )
POOL_CREATE_PARAMS=( "--write-layout 512" "" "--layout test_layout" )
POOL_CHECK_PARAMS=( "-smgB" "-s" "-soOaAbZH -l -C" )
POOL_OBJ=2

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
