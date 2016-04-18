#
# Copyright 2015-2016, Intel Corporation
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
# src/test/blk_nblock/TEST0 -- unit test for pmemblk_nblock
#
$Env:UNITTEST_NAME = "blk_nblock\TEST0"
$Env:UNITTEST_NUM = "0"
# Pl TODO:  bash has a few calls to tools that we don't have on
# windows (yet) that set PMEM_IS_PMEM and NON_PMEM_IS_PMEM based
# on their outpute
$Env:PMEM_IS_PMEM = $true
$Env:NON_PMEM_IS_PMEM = $true
$DIR = ""

# standard unit test setup
. ..\unittest\unittest.ps1

require_fs_type "local"

# PL TODO:  need to find eq of linux overcommit mem, runs out pretty quick
# with this test on windows so larger tests are commented out below
require_unlimited_vm

setup

#
# Create a gajillion files, each file size created
# has six versions for the six block sizes being tested.
# Except for testfile1, since that should fail because
# it is too small.
#
# These are holey files, so they actually don't take up
# any significant space.
#
create_holey_file 2 $DIR\testfile1
create_holey_file 2048 $DIR\testfile2.512
create_holey_file 2048 $DIR\testfile2.520
create_holey_file 2048 $DIR\testfile2.528
create_holey_file 2048 $DIR\testfile2.4096
create_holey_file 2048 $DIR\testfile2.4160
create_holey_file 2048 $DIR\testfile2.4224
#create_holey_file 536576 $DIR\testfile3.512
#create_holey_file 536576 $DIR\testfile3.520
#create_holey_file 536576 $DIR\testfile3.528
#create_holey_file 536576 $DIR\testfile3.4096
#create_holey_file 536576 $DIR\testfile3.4160
#create_holey_file 536576 $DIR\testfile3.4224
#create_holey_file 549755822080 $DIR\testfile4.512
#create_holey_file 549755822080 $DIR\testfile4.520
#create_holey_file 549755822080 $DIR\testfile4.528
#create_holey_file 549755822080 $DIR\testfile4.4096
#create_holey_file 549755822080 $DIR\testfile4.4160
#create_holey_file 549755822080 $DIR\testfile4.4224
#create_holey_file 513G $DIR\testfile5.512
#create_holey_file 513G $DIR\testfile5.520
#create_holey_file 513G $DIR\testfile5.528
#create_holey_file 513G $DIR\testfile5.4096
#create_holey_file 513G $DIR\testfile5.4160
#create_holey_file 513G $DIR\testfile5.4224
#create_holey_file 514G $DIR\testfile6.512
#create_holey_file 514G $DIR\testfile6.520
#create_holey_file 514G $DIR\testfile6.528
#create_holey_file 514G $DIR\testfile6.4096
#create_holey_file 514G $DIR\testfile6.4160
#create_holey_file 514G $DIR\testfile6.4224
# MINIMUM POOL SIZE = 16MB + 8KB
$MIN_POOL_SIZE = "16+8"
create_holey_file $MIN_POOL_SIZE $DIR\testfile7.512
create_holey_file $MIN_POOL_SIZE $DIR\testfile7.520
create_holey_file $MIN_POOL_SIZE $DIR\testfile7.528
create_holey_file $MIN_POOL_SIZE $DIR\testfile7.4096
create_holey_file $MIN_POOL_SIZE $DIR\testfile7.4160
create_holey_file $MIN_POOL_SIZE $DIR\testfile7.4224

# should fail:
#	512:$DIR\testfile1 (file is too small)
#	4096:$DIR\testfile2.512 (bsize doesn't match pool)
expect_normal_exit ..\..\x64\debug\blk_nblock$Env:EXESUFFIX `
	512:$DIR\testfile1 `
	512:$DIR\testfile2.512 `
	4096:$DIR\testfile2.512 `
	520:$DIR\testfile2.520 `
	528:$DIR\testfile2.528 `
	4096:$DIR\testfile2.4096 `
	4160:$DIR\testfile2.4160 `
	#4224:$DIR\testfile2.4224 `
	#512:$DIR\testfile3.512 `
	#520:$DIR\testfile3.520 `
	#528:$DIR\testfile3.528 `
	#4096:$DIR\testfile3.4096 ` 
	#4160:$DIR\testfile3.4160 `
	#4224:$DIR\testfile3.4224 `
	#512:$DIR\testfile4.512 `
	#520:$DIR\testfile4.520 `
	#528:$DIR\testfile4.528 ` 
	#4096:$DIR\testfile4.4096 `
	#4160:$DIR\testfile4.4160 `
	#4224:$DIR\testfile4.4224 `
	#512:$DIR\testfile5.512 
	#520:$DIR\testfile5.520 `
	#528:$DIR\testfile5.528 ` 
	#4096:$DIR\testfile5.4096 `
	#4160:$DIR\testfile5.4160 `
	#4224:$DIR\testfile5.4224 `
	#512:$DIR\testfile6.512 `
	#520:$DIR\testfile6.520 `
	#528:$DIR\testfile6.528 `
	#4096:$DIR\testfile6.4096 `
	#4160:$DIR\testfile6.4160 `
	#4224:$DIR\testfile6.4224 `
	#512:$DIR\testfile7.512 `
	#520:$DIR\testfile7.520 `
	#528:$DIR\testfile7.528 `
	#4096:$DIR\testfile7.4096 `
	#4160:$DIR\testfile7.4160 
	#4224:$DIR\testfile7.4224 `

# check will print the appropriate pass/fail message
check
