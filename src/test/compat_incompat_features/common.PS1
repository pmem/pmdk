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
