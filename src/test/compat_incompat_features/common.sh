#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2023, Intel Corporation
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

POOL_TYPES=(obj)

# pmempool create arguments:
declare -A create_args
create_args[obj]="obj $POOLSET"

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
