#
# Copyright 2014-2019, Intel Corporation
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
# src/test/pmem_map_file_win/TEST1 -- unit test for pmem_map_file_win
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

# <path> <size> <flags> <mode> <use_mlen> <use_is_pmem>

# pass the file of zero length
expect_normal_exit $Env:EXE_DIR\pmem_map_file_win$Env:EXESUFFIX `
    $DIR\testfile1 0 - 0666 1 1 `
    $DIR\testfile2 0 C 0666 0 0 `
    $DIR\testfile3 4096 C 0666 1 1 `
    $DIR\testfile4 0 E 0666 1 1 `
    $DIR\testfile5 4096 E 0666 1 1 `
    $DIR\testfile6 4096 CE 0666 1 1 `
    $DIR\testfile7 4096 CES 0666 1 1 `
    $DIR\testfile8 4096 CS 0666 1 1 `
    $DIR\testfile9 -1 C 0666 1 1 `
    $DIR\testfile10 4096 X 0666 1 1 `
    $DIR\testfile11 4096 CX 0666 1 1 `
    $DIR\testfile12 0x1FFFFFFFFFFFFFFF CE 0666 1 1 `
    $DIR 0 T 0666 1 1 `
    $DIR 4096 T 0666 1 1 `
    $DIR 4096 TC 0666 1 1 `
    $DIR 4096 TE 0666 1 1 `
    $DIR 4096 TS 0666 1 1 `
    $DIR 4096 TSE 0666 1 1 `
    $DIR 4096 TCE 0666 1 1 `
    $DIR 4096 TSEC 0666 1 1 `
    \nul\nonexistingdir 4096 T 0666 1 1

check_files $DIR\testfile3 `
    $DIR\testfile6 `
    $DIR\testfile7 `
    $DIR\testfile8

check_no_files $DIR\testfile1 `
    $DIR\testfile2 `
    $DIR\testfile4 `
    $DIR\testfile5 `
    $DIR\testfile9 `
    $DIR\testfile10 `
    $DIR\testfile11 `
    $DIR\testfile12

check

pass
