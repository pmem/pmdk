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
# pmempool_check/common.sh -- checking pools helpers
#

LOG=out${UNITTEST_NUM}.log
rm -f $LOG && touch $LOG

LAYOUT=OBJ_LAYOUT$SUFFIX
POOLSET=$DIR/poolset

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
