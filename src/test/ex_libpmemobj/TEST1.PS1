#
# Copyright 2015-2019, Intel Corporation
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
# src/test/ex_libpmemobj/TEST1 -- unit test for libpmemobj examples
#

. ../unittest/unittest.PS1

require_test_type medium
require_build_type debug nondebug
require_no_unicode

setup

expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_btree $DIR\testfile1 i 1234 foo > out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_btree $DIR\testfile1 i 4321 bar >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_btree $DIR\testfile1 i 1 Hello >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_btree $DIR\testfile1 i 2 World! >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_btree $DIR\testfile1 p >> out$Env:UNITTEST_NUM.log 2>&1
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_btree $DIR\testfile1 f 4321 >> out$Env:UNITTEST_NUM.log 2>&1

check

pass
