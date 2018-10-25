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
# bttdevice/TEST0 -- test for checking bttcreate tool
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out${Env:UNITTEST_NUM}.log"
rm $LOG -Force -ea si
touch $LOG

echo "Create btt device - uuid defined by the user" >> $LOG
expect_normal_exit $BTTCREATE -s '20M' -b 1K -l 256 `
		-u "9047cad6-53e1-4ba8-bcdd-a9fb411f7392" -v $POOL >> $LOG

expect_normal_exit $PMEMPOOL info $POOL >> $LOG
check_file $POOL
rm $POOL -Force -ea si

echo "`nCreate btt device - automatic uuid generation" >> $LOG
expect_normal_exit $BTTCREATE -s 40M -b 2K -l 256 -v $POOL >> $LOG

expect_normal_exit $PMEMPOOL info $POOL >> $LOG
check_file $POOL

check

pass
