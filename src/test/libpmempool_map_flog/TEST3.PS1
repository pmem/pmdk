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
# libpmempool_map_flog/TEST3 -- test for checking map and flog
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )

$Env:UNITTEST_NAME = "libpmempool_map_flog\TEST3"
$Env:UNITTEST_NUM = "3"

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

$ent_val = 10

expect_normal_exit $BTTCREATE $POOL

for ($i=0; $i -lt $ent_val; $i++) {
    $spcmd = "bttdevice.arena.btt_map($i)=0xC000000${i}"
    Write-Output $spcmd | Out-File -append -encoding ascii $LOG_TEMP
    &$PMEMSPOIL $POOL $spcmd
}

$spcmd = "bttdevice.arena.btt_map(5)=0xC0000002"
Write-Output $spcmd | Out-File -append -encoding ascii $LOG_TEMP
&$PMEMSPOIL $POOL $spcmd

$spcmd = "bttdevice.arena.btt_map(6)=0xC0000002"
Write-Output $spcmd | Out-File -append -encoding ascii $LOG_TEMP
&$PMEMSPOIL $POOL $spcmd

$spcmd = "bttdevice.arena.btt_map(7)=0xC0000002"
Write-Output $spcmd | Out-File -append -encoding ascii $LOG_TEMP
&$PMEMSPOIL $POOL $spcmd

$spcmd = "bttdevice.arena.btt_map(10)=0xC0000002"
Write-Output $spcmd | Out-File -append -encoding ascii $LOG_TEMP
&$PMEMSPOIL $POOL $spcmd

$spcmd = "bttdevice.arena.btt_map(11)=0xC0000002"
Write-Output $spcmd | Out-File -append -encoding ascii $LOG_TEMP
&$PMEMSPOIL $POOL $spcmd

$spcmd = "bttdevice.arena.btt_map(12)=0xC0000003"
Write-Output $spcmd | Out-File -append -encoding ascii $LOG_TEMP
&$PMEMSPOIL $POOL $spcmd

expect_normal_exit $EXE -r 1 -t btt -a 1 $POOL
Get-content -Encoding Ascii $LOG | Out-File -append -encoding ascii $LOG_TEMP

expect_normal_exit $PMEMPOOL info $POOL -f btt -m | `
    Select-String -encoding ASCII -Pattern "error" | `
    ForEach-Object {$_.Line} | Out-File -append -encoding ascii $LOG_TEMP

Move-Item -Force $LOG_TEMP $LOG
check_file $POOL

check
pass
