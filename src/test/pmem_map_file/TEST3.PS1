# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

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
