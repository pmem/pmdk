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
# pmempool_check/common.sh -- checking pools helpers
#

LOG=out${UNITTEST_NUM}.log
rm -f $LOG && touch $LOG

LAYOUT=OBJ_LAYOUT$SUFFIX
POOLSET=$DIR/poolset

# create_poolset_2replicas -- create poolset with two replicas
function create_poolset_2replicas() {
	create_poolset $POOLSET \
				   8M:$DIR/part00:x \
				   r \
				   8M:$DIR/part10:x
}

# sds_corrupt_replica -- corrupt shutdown state
#
#	usage: sds_corrupt_replica <replica>
function sds_corrupt_replica() {
	local replica=$1
	expect_normal_exit $PMEMSPOIL --replica $replica $POOLSET \
					   pool_hdr.shutdown_state.usc=999 \
					   pool_hdr.shutdown_state.dirty=1 \
					   "pool_hdr.shutdown_state.checksum_gen\(\)"
}

# unittest_sds_init -- create poolset with two replicas and corrupt sds on both
function unittest_sds_init() {
	create_poolset_2replicas
	expect_normal_exit $PMEMPOOL$EXESUFFIX create --layout=$LAYOUT obj $POOLSET
	sds_corrupt_replica 0
	sds_corrupt_replica 1
}

# unittest_sds -- perform shutdown state unittest
#
#	usage: unittest_sds <scenario>
function unittest_sds() {
	expect_abnormal_exit $PMEMPOOL$EXESUFFIX check $POOLSET >> $LOG
	exit_func=expect_normal_exit

	case "$1" in
	fix_second_replica_only)
		echo -e "n\ny\n" | expect_normal_exit $PMEMPOOL$EXESUFFIX check -vr $POOLSET >> $LOG
		;;
	fix_first_replica)
		echo -e "y\n" | expect_normal_exit $PMEMPOOL$EXESUFFIX check -vr $POOLSET >> $LOG
		;;
	fix_no_replicas)
		echo -e "n\nn\n" | expect_abnormal_exit $PMEMPOOL$EXESUFFIX check -vr $POOLSET >> $LOG
		exit_func=expect_abnormal_exit
		;;
	*)
		fatal "unittest_sds: undefined scenario '$1'"
		;;
	esac

	$exit_func $PMEMPOOL$EXESUFFIX check $POOLSET >> $LOG
}
