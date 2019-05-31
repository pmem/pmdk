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
# src/test/pmem_map_file/TEST3 -- unit test for pmem_map_file
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

function is_sparse(){
	$file = $args[0]
	$res = fsutil sparse queryflag $file

	if ($res -match "This file is NOT set as sparse"){
		fail "error: file is not sparse"
	}
}

create_file 4K $DIR/testfile2
create_file 4K $DIR/testfile3

create_holey_file 4K $DIR/sparsefile1

# <path> <size> <flags> <mode> <use_mlen> <use_is_pmem> <err_code>
expect_normal_exit $Env:EXE_DIR\pmem_map_file$Env:EXESUFFIX `
	$DIR/testfile1 0 0 0666 nullptr nullptr 0 `
	$DIR/testfile2 4096 CE 0666 1 1 EEXIST `
	$DIR/testfile3 4096 CES 0666 1 1 EEXIST `
	$DIR/sparsefile1 0 CS 0666 1 1 0 `
	\nul\nonexistingdir 0 0 0666 1 1 ENOENT `
	\nul\nonexistingdir 4096 CET 0666 1 1 ENOENT

is_sparse $DIR/sparsefile1

check

pass
