#
# Copyright 2015-2018, Intel Corporation
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
# src/test/obj_redo_log/TEST4 -- unit test for redo_log_recover function
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("d")]
    $DIR = ""
    )


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
	R `
	s:0:0x00004008:0x01010101 `
	r:0x00004008 `
	R `
	r:0x00004008 `
	e:0 `
	C `
	f:0:0x00004008:0x01010101 `
	r:0x00004008 `
	R `
	r:0x00004008 `
	e:0 `
	C `
	s:0:0x00004010:0x11111111 `
	s:1:0x00004018:0x22222222 `
	s:2:0x00004020:0x33333333 `
	r:0x00004010 `
	r:0x00004018 `
	r:0x00004020 `
	R `
	r:0x00004010 `
	r:0x00004018 `
	r:0x00004020 `
	e:0 `
	e:1 `
	e:2 `
	C `
	s:0:0x00004010:0x11111111 `
	s:1:0x00004018:0x22222222 `
	s:2:0x00004020:0x33333333 `
	f:3:0x00004028:0x44444444 `
	r:0x00004010 `
	r:0x00004018 `
	r:0x00004020 `
	r:0x00004028 `
	R `
	r:0x00004010 `
	r:0x00004018 `
	r:0x00004020 `
	r:0x00004028 `
	e:0 `
	e:1 `
	e:2 `
	e:3 `
	C

check

pass
