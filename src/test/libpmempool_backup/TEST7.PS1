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
# libpmempool_backup/TEST7 -- test backup to pool which already exists
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )

$Env:UNITTEST_NAME = "libpmempool_backup/TEST7"
$Env:UNITTEST_NUM = "7"

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type pmem non-pmem

setup

. ./common.PS1

for ($i=0; $i -lt $POOL_TYPES.Count; $i++ ) {
	backup_cleanup

	$POOLFILE= -join("$DIR/pool.", $POOL_TYPES[$i]);
	Remove-Item $POOLFILE -Force -ea si

	# create source poolset
	expect_normal_exit $PMEMPOOL create $POOL_TYPES[$i] `
		$POOL_CREATE_PARAMS[$i] --size 20M $POOLFILE

	# create backup pool file
	truncate -s 20M $POOLFILE$BACKUP
	backup_and_compare $POOLFILE $POOL_TYPES[$i]

	# create too small backup pool file
	truncate -s 10M $POOLFILE$BACKUP
	backup_and_compare $POOLFILE $POOL_TYPES[$i]
}
Remove-Item $OUT -Force -ea si
Move-Item $OUT_TEMP $OUT

check

pass
