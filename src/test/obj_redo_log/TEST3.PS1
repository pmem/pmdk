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
# src/test/obj_redo_log/TEST3 -- unit test for redo_log_process function
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )
$Env:UNITTEST_NAME = "obj_redo_log/TEST3"
$Env:UNITTEST_NUM = "3"


# standard unit test setup
. ..\unittest\unittest.ps1

require_test_type medium

require_build_type debug

setup

$FILE=$DIR+"\pool"
$FSIZE=1024 * 1024
$RSIZE=4

create_holey_file ($FSIZE.ToString() + "b") $FILE

expect_normal_exit $Env:EXE_DIR\obj_redo_log$Env:EXESUFFIX $FILE $RSIZE `
	C `
	f:0:0x00003008:0x01010101 `
	r:0x00003008 `
	P `
	r:0x00003008 `
	e:0 `
	s:0:0x00003010:0x11111111 `
	s:1:0x00003018:0x22222222 `
	s:2:0x00003020:0x33333333 `
	f:3:0x00003028:0x44444444 `
	r:0x00003010 `
	r:0x00003018 `
	r:0x00003020 `
	r:0x00003028 `
	P `
	r:0x00003010 `
	r:0x00003018 `
	r:0x00003020 `
	r:0x00003028 `
	e:0 `
	e:1 `
	e:2 `
	e:3 `
	C `
	s:0:0x00003100:0xaaaaaaaa `
	s:1:0x00003108:0xbbbbbbbb `
	s:2:0x00003110:0xcccccccc `
	f:3:0x00003118:0xdddddddd `
	r:0x00003100 `
	r:0x00003108 `
	r:0x00003110 `
	r:0x00003118 `
	P `
	r:0x00003100 `
	r:0x00003108 `
	r:0x00003110 `
	r:0x00003118 `
	e:0 `
	e:1 `
	e:2 `
	e:3 `
	C

check

pass
