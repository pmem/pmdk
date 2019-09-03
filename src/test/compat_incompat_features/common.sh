#!/usr/bin/env bash
#
# Copyright 2017-2019, Intel Corporation
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
# compat_incompat_features/common.sh -- common stuff for compat/incompat feature
#                                       flags tests
#
ERR=err${UNITTEST_NUM}.log
ERR_TEMP=err${UNITTEST_NUM}_part.log
LOG=out${UNITTEST_NUM}.log
LOG_TEMP=out${UNITTEST_NUM}_part.log
rm -f $LOG && touch $LOG
rm -f $LOG_TEMP && touch $LOG_TEMP
rm -f $ERR && touch $ERR
rm -f $ERR_TEMP && touch $ERR_TEMP

LAYOUT=OBJ_LAYOUT$SUFFIX
POOLSET=$DIR/pool.set

POOL_TYPES=(obj blk log)

# pmempool create arguments:
declare -A create_args
create_args[obj]="obj $POOLSET"
create_args[blk]="blk 512 $POOLSET"
create_args[log]="log $POOLSET"

# Known compat flags:

# Known incompat flags:
let "POOL_FEAT_SINGLEHDR = 0x0001"
let "POOL_FEAT_CKSUM_2K = 0x0002"
let "POOL_FEAT_SDS = 0x0004"

# Unknown compat flags:
UNKNOWN_COMPAT=(2 4 8 1024)

# Unknown incompat flags:
UNKNOWN_INCOMPAT=(8 15 1111)

# set compat flags in header
set_compat() {
	local part=$1
	local flag=$2
	expect_normal_exit $PMEMSPOIL $part pool_hdr.features.compat=$flag \
		"pool_hdr.f:checksum_gen"
}

# set incompat flags in header
set_incompat() {
	local part=$1
	local flag=$2
	expect_normal_exit $PMEMSPOIL $part pool_hdr.features.incompat=$flag \
		"pool_hdr.f:checksum_gen"
}
