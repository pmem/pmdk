#
# Copyright 2015-2017, Intel Corporation
# Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
# src/test/out_err_mt/TEST2 -- unit test for error messages
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "out_err_mt/TEST2"
$Env:UNITTEST_NUM = "2"


# standard unit test setup
. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

# XXX: we don't have valgrind in Windows yet
# require_valgrind_dev_version 3.7
# configure_valgrind helgrind force-enable

setup

$Env:PMEM_LOG_LEVEL = $null
$Env:PMEMLOG_LOG_LEVEL = $null
$Env:PMEMBLK_LOG_LEVEL = $null
$Env:PMEMOBJ_LOG_LEVEL = $null
$Env:VMEM_LOG_LEVEL = $null

$Env:PMEM_LOG_FILE = $null
$Env:PMEMLOG_LOG_FILE = $null
$Env:PMEMBLK_LOG_FILE = $null
$Env:PMEMOBJ_LOG_FILE = $null
$Env:VMEM_LOG_FILE = $null

expect_normal_exit $Env:EXE_DIR\out_err_mt$Env:EXESUFFIX `
	$DIR\testfile1 $DIR\testfile2 $DIR\testfile3 $DIR

check

pass
