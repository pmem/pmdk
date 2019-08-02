#
# Copyright 2019, Intel Corporation
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
# pmempool_create/TEST14 -- test pmemobj pool creation with default and custom layout
# using increased verbosity level
#

. ../unittest/unittest.ps1

require_test_type medium

require_fs_type any

setup

$LOG = "out${Env:UNITTEST_NUM}.log"
$ERR_LOG = "err${Env:UNITTEST_NUM}.log"

# test default layout
expect_normal_exit $PMEMPOOL$EXESUFFIX create obj -v $DIR\pool.obj > $LOG

# test custom layout
expect_normal_exit $PMEMPOOL$EXESUFFIX create obj -v --layout=moose $DIR\pool2.obj >> $LOG

# test max layout

# MAX_LAYOUT is equal to PMEMOBJ_MAX_LAYOUT (i.e. 1024) macro's value
# reduced by 1 for terminating null byte
$MAX_LAYOUT = 1023

$LAYOUT = "x" * $MAX_LAYOUT

expect_normal_exit $PMEMPOOL$EXESUFFIX create obj -v --layout=$LAYOUT $DIR/pool3.obj >> $LOG

# test too long layout

$INVALID_LAYOUT= "x" * ($MAX_LAYOUT + 1)

expect_abnormal_exit cmd /c $PMEMPOOL$EXESUFFIX create obj -v --layout=$INVALID_LAYOUT $DIR/pool4.obj `>`> $LOG 2`>$ERR_LOG

check

pass
