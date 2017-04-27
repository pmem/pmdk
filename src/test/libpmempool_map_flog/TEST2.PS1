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
# libpmempool_map_flog/TEST2 -- test for checking map and flog
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )

$Env:UNITTEST_NAME = "libpmempool_map_flog\TEST2"
$Env:UNITTEST_NUM = "2"

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$POOL = "$DIR/file.pool"
$LOG = "out${Env:UNITTEST_NUM}.log"
$LOG_TEMP = "out${Env:UNITTEST_NUM}_part.log"
Remove-Item $LOG -Force -ea si
touch $LOG
Remove-Item $LOG_TEMP -Force -ea si
touch $LOG_TEMP

$EXE = "$Env:EXE_DIR\libpmempool_test$Env:EXESUFFIX"

$map_err = @{ "Initial"="0"
              "Error"="4"
              "Zeroed"="8"
              "Normal"="C" }

$ent_val = 9

foreach ($field in ("Zeroed", "Error", "Initial", "Normal")) {
    expect_normal_exit $BTTCREATE $POOL

    $x = $map_err[$field]
    for ($i=0; $i -lt $ent_val; $i++) {
        $spcmd = "bttdevice.arena.btt_map($i)=0x${x}000000${i}"
        Write-Output "${field}: $spcmd" | Out-File -append -encoding ascii $LOG_TEMP
        &$PMEMSPOIL $POOL $spcmd
    }

    $spcmd = "bttdevice.arena.btt_flog(3).seq=4"
    Write-Output "${field}: $spcmd" | Out-File -append -encoding ascii $LOG_TEMP
    &$PMEMSPOIL $POOL $spcmd

    $spcmd = "bttdevice.arena.btt_flog(4).old_map=0x${x}000FFFF"
    Write-Output "${field}: $spcmd" | Out-File -append -encoding ascii $LOG_TEMP
    &$PMEMSPOIL $POOL $spcmd

    $spcmd = "bttdevice.arena.btt_flog(5).new_map=0x${x}000FFFF"
    Write-Output "${field}: $spcmd" | Out-File -append -encoding ascii $LOG_TEMP
    &$PMEMSPOIL $POOL $spcmd

    $spcmd = "bttdevice.arena.btt_flog(6).old_map=0x${x}0000001"
    Write-Output "${field}: $spcmd" | Out-File -append -encoding ascii $LOG_TEMP
    &$PMEMSPOIL $POOL $spcmd

    $spcmd = "bttdevice.arena.btt_flog(7).new_map=0x${x}0000002"
    Write-Output "${field}: $spcmd" | Out-File -append -encoding ascii $LOG_TEMP
    &$PMEMSPOIL $POOL $spcmd

    $spcmd = "bttdevice.arena.btt_flog(8).new_map=0x${x}0000069"
    Write-Output "${field}: $spcmd" | Out-File -append -encoding ascii $LOG_TEMP
    &$PMEMSPOIL $POOL $spcmd

    expect_normal_exit $EXE -r 1 -t btt -a 1 $POOL
    Get-Content -Encoding Ascii $LOG | Out-File -append -encoding ascii $LOG_TEMP
}

Move-Item -Force $LOG_TEMP $LOG
check_file $POOL

check
pass
