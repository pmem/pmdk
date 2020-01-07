# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation
#
$LOG = "out${Env:UNITTEST_NUM}.log"
$LOG_TEMP = "out${Env:UNITTEST_NUM}_part.log"
$ERR = "err${Env:UNITTEST_NUM}.log"
$ERR_TEMP = "err${Env:UNITTEST_NUM}_part.log"
rm $LOG -Force -ea si
touch $LOG
rm $LOG_TEMP -Force -ea si
touch $LOG_TEMP
rm $ERR -Force -ea si
touch $ERR
rm $ERR_TEMP -Force -ea si
touch $ERR_TEMP

$LAYOUT = "OBJ_LAYOUT${Env:SUFFIX}"
$POOLSET = "$DIR\pool.set"

$POOL_TYPES = "obj", "blk", "log"

# pmempool create arguments:
$create_args = @{}
$create_args.obj = @("obj", "$POOLSET")
$create_args.blk = @("blk", "512", "$POOLSET")
$create_args.log = @("log", "$POOLSET")

# Known compat flags:

# Known incompat flags:
$POOL_FEAT_SINGLEHDR = 0x0001
$POOL_FEAT_CKSUM_2K = 0x0002

# Unknown compat flags:
$UNKNOWN_COMPAT = 2, 4, 8, 1024

# Unknown incompat flags:
$UNKNOWN_INCOMPAT = 8, 15, 1111

# set compat flags in header
function set_compat {
    Param (
        [parameter(Position=0)]
        [String]$part,
        [parameter(Position=1)]
        [String]$flag

    )
    expect_normal_exit $PMEMSPOIL $part "pool_hdr.features.compat=$flag" `
        "pool_hdr.f:checksum_gen"
}

# set incompat flags in header
function set_incompat {
    Param (
        [parameter(Position=0)]
        [String]$part,
        [parameter(Position=1)]
        [String]$flag

    )
    expect_normal_exit $PMEMSPOIL $part "pool_hdr.features.incompat=$flag" `
        "pool_hdr.f:checksum_gen"
}
