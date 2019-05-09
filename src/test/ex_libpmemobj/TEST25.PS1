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
# src\test\ex_libpmemobj\TEST25.PS1 -- unit test for libpmemobj examples
#

# standard unit test setup
. ..\unittest\unittest.PS1

require_test_type medium
require_build_type debug nondebug
require_no_unicode

setup

Copy-Item -Path ..\..\examples\libpmemobj\colony\ints.txt -Destination int.txt
Copy-Item -Path ..\..\examples\libpmemobj\colony\pmemoids.txt -Destination pmemoid.txt

# create-colony: big usage, not enough args
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony create-colony not_enough_args_here > out$Env:UNITTEST_NUM.log
# create-colony: big usage, unknown operation
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colonyy coli int 27 >> out$Env:UNITTEST_NUM.log
# create-colony: usage, too many args
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colony too many args here >> out$Env:UNITTEST_NUM.log
# create-colony: unknown type
& expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colony colu unknown_type 12 2> $null

# creation colony of ints (block capacity = 5)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colony coli int 5 >> out$Env:UNITTEST_NUM.log

# insertion 20 ints from the file, allocation of 4 blocks
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int-from-file coli int.txt >> out$Env:UNITTEST_NUM.log

# insert wrong type
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid coli 22378745 47874446 2>&1 > $null

# removal one item (idx = 15, 13, 14, 12, 6, 5), indexes go to the list of free indexes
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 15 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 13 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 14 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 12 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 6 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 5 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content coli >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony coli >> out$Env:UNITTEST_NUM.log

# 2x removal range (idx = 5, 6, 14, 15 does not exist, idx = 20 is out of range), indexes go to the list of free indexes,
# last and second blocks are freed (in this order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range coli 14 20 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range coli 5 10 >> out$Env:UNITTEST_NUM.log

# insert one int (idx = 10), index from the list of free indexes
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int coli 183 >> out$Env:UNITTEST_NUM.log

# deletion free blocks (2), last and second blocks are deleted (in reverse order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-free-blocks coli >> out$Env:UNITTEST_NUM.log

# insertion 20 ints from the file, 3 indexes from the list of free indexes (idx = 7, 9, 8) and allocation of 4 blocks
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int-from-file coli int.txt >> out$Env:UNITTEST_NUM.log

# removal one item (idx = 10, 8, 9), indexes go to the list of free indexes, removal range, indexes go to the list of free indexes,
# third block is freed, insertion one int
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 10 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 8 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 9 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range coli 8 16 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-free-blocks coli >> out$Env:UNITTEST_NUM.log

# deletion colony
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-colony coli >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content coli >> out$Env:UNITTEST_NUM.log

# creation colony of PMEMoids (block capacity = 7)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colony colp PMEMoid 7 >> out$Env:UNITTEST_NUM.log

# insertion 20 PMEMoids from the file, allocation of 3 blocks
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid-from-file colp pmemoid.txt >> out$Env:UNITTEST_NUM.log
# insert wrong type
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int colp 19 2>&1 > $null

# 2x removal range, (idx = 8, 10 do not exist), removal one item (idx = 15, 14) indexes go to the list of free indexes,
# second and first blocks are freed (in this order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 6 13 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 0 5 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item colp 15 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item colp 14 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony colp >> out$Env:UNITTEST_NUM.log

# deletion free blocks (2), second and first blocks are deleted (in reverse order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-free-blocks colp >> out$Env:UNITTEST_NUM.log

# insertion PMEMoids (20 from file and 4 single), 2 indexes from the list of free indexes, 1 index as unfilled before,
# allocation of 3 blocks
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid-from-file colp pmemoid.txt >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid colp 40731435551616 985633251211 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid colp 35551698674407316 14073705021046740 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid colp 98674447107335551 777441113705002 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid colp 5858755551616 425370962235 >> out$Env:UNITTEST_NUM.log

# removal range, one before last and last blocks are freed (in this order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 13 27 >> out$Env:UNITTEST_NUM.log

# deletion free blocks (2), one before last and last blocks are deleted (in reverse order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-free-blocks colp >> out$Env:UNITTEST_NUM.log

# removal range, indexes go to the list of free indexes, first block is freed
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 0 12 >> out$Env:UNITTEST_NUM.log

# insertion 20 PMEMoids from the file, 2 blocks are taken from the list of free blocks
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid-from-file colp pmemoid.txt >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid colp 12596521616 40000900235 >> out$Env:UNITTEST_NUM.log

# 2x removal range, indexes go to the list of free indexes, first and last blocks are freed (in this order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 0 9 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 13 20 >> out$Env:UNITTEST_NUM.log

# deletion free blocks (2), first and last blocks are deleted (in reverse order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-free-blocks colp >> out$Env:UNITTEST_NUM.log

# insertion 20 PMEMoids from the file, removal range, indexes go to the list of free indexes, first and second blocks are freed (in this order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid-from-file colp pmemoid.txt >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 0 18 >> out$Env:UNITTEST_NUM.log

# deletion colony
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-colony colp >> out$Env:UNITTEST_NUM.log

Remove-Item int.txt -Force
Remove-Item pmemoid.txt -Force

check

pass
