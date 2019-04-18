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

New-Item file_without_permissions
attrib +r file_without_permissions

New-Item file_with_changing_permissions

Copy-Item -Path ..\..\examples\libpmemobj\colony\ints.txt -Destination int.txt
Copy-Item -Path ..\..\examples\libpmemobj\colony\pmemoids.txt -Destination pmemoid.txt

# big usage, not enough args
 expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony not_enough_args > out$Env:UNITTEST_NUM.log

# create-colony: big usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colony >> out$Env:UNITTEST_NUM.log
# create-colony: usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colony too may args here >> out$Env:UNITTEST_NUM.log
# create-colony: file without permissions
expect_abnormal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\file_without_permissions\file create-colony int 20 >> out$Env:UNITTEST_NUM.log
# create-colony: unknown type
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colony colu unknown_type 12 >> out$Env:UNITTEST_NUM.log
# create-colony: requested size > MAX
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colony colu int 163792271970 >> out$Env:UNITTEST_NUM.log

# insert-int: big usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int >> out$Env:UNITTEST_NUM.log
# insert-int: usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int too may args here >> out$Env:UNITTEST_NUM.log
# insert-int: colony does not exist
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int colx 90 >> out$Env:UNITTEST_NUM.log
# insert-int: wrong type
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\file_with_changing_permissions create-colony colu int 14 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\file_with_changing_permissions insert-PMEMoid colu 22378745 47874446 >> out$Env:UNITTEST_NUM.log
# insert-int: file without permissions
attrib +r file_with_changing_permissions
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\file_with_changing_permissions insert-int colu 3 >> out$Env:UNITTEST_NUM.log

# insert-PMEMoid: big usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid >> out$Env:UNITTEST_NUM.log
# insert-PMEMoid: usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid too may args here >> out$Env:UNITTEST_NUM.log
# insert-PMEMoid: colony does not exist
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid colx 45797851 2235666 >> out$Env:UNITTEST_NUM.log
# insert-PMEMoid: file without permissions
attrib -r file_with_changing_permissions
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\file_with_changing_permissions create-colony colu PMEMoid 9 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\file_with_changing_permissions insert-int colu 19 >> out$Env:UNITTEST_NUM.log
# insert-PMEMoid: file without permissions
attrib +r file_with_changing_permissions
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\file_with_changing_permissions insert-PMEMoid colu 378787878 89898989 >> out$Env:UNITTEST_NUM.log

# insert-int-from-file: big usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int-from-file >> out$Env:UNITTEST_NUM.log
# insert-int-from-file: usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int-from-file too may args here >> out$Env:UNITTEST_NUM.log
# insert-int-from-file: colony does not exist
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int-from-file colx int.txt >> out$Env:UNITTEST_NUM.log
# insert-int-from-file: txt file without permissions
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colony coln int 6 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int-from-file coln txt_file_without_permissions.txt >> out$Env:UNITTEST_NUM.log

# insert-PMEMoid-from-file: big usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid-from-file >> out$Env:UNITTEST_NUM.log
# insert-PMEMoid-from-file: usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid-from-file too may args here >> out$Env:UNITTEST_NUM.log
# insert-PMEMoid-from-file: colony does not exist
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid-from-file colx pmemoid.txt >> out$Env:UNITTEST_NUM.log
# insert-PMEMoid-from-file: txt file without permissions
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colony coln PMEMoid 6 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid-from-file coln txt_file_without_permissions.txt >> out$Env:UNITTEST_NUM.log

# remove-item: big usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item >> out$Env:UNITTEST_NUM.log
# remove-item: usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item too may args here >> out$Env:UNITTEST_NUM.log
# remove-item: colony does not exist
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item colx 15 >> out$Env:UNITTEST_NUM.log

# remove-range: big usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range >> out$Env:UNITTEST_NUM.log
# remove-range: usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range too may args here >> out$Env:UNITTEST_NUM.log
# remove-range: colony does not exist
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colx 11 33 >> out$Env:UNITTEST_NUM.log

# print-content: big usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content >> out$Env:UNITTEST_NUM.log
# print-content: usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content too may args here >> out$Env:UNITTEST_NUM.log
# print-content: colony does not exist
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colx >> out$Env:UNITTEST_NUM.log

# print-colony: big usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony >> out$Env:UNITTEST_NUM.log
# print-colony: usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony too may args here >> out$Env:UNITTEST_NUM.log
# print-colony: colony does not exist
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony colx >> out$Env:UNITTEST_NUM.log

# delete-free-blocks: big usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-free-blocks >> out$Env:UNITTEST_NUM.log
# delete-free-blocks: usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-free-blocks too may args here >> out$Env:UNITTEST_NUM.log
# delete-free-blocks: colony does not exist
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-free-blocks colx >> out$Env:UNITTEST_NUM.log

# delete-colony: big usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-colony >> out$Env:UNITTEST_NUM.log
# delete-colony: usage
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-colony too may args here >> out$Env:UNITTEST_NUM.log
# delete-colony: colony does not exist
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-colony colx >> out$Env:UNITTEST_NUM.log

# big usage, unknown operation
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile unknown_operation >> out$Env:UNITTEST_NUM.log

# standard use
# creation colony of ints (block capacity = 5)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colony coli int 5 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content coli >> out$Env:UNITTEST_NUM.log

# insertion 20 ints from the file, allocation of 4 blocks
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int-from-file coli int.txt >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content coli >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony coli >> out$Env:UNITTEST_NUM.log

# removal one item (idx = 15, 13, 14, 12), indexes go to the list of free indexes
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 15 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 13 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 14 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-item coli 12 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content coli >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony coli >> out$Env:UNITTEST_NUM.log

# 2x removal range (idx = 14, 15 does not exist, idx = 20 is out of range), indexes go to the list of free indexes,
# last and second blocks are freed (in this order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range coli 14 20 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range coli 5 10 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content coli >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony coli >> out$Env:UNITTEST_NUM.log

# insert one int (idx = 10), index from the list of free indexes
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int coli 183 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content coli >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony coli >> out$Env:UNITTEST_NUM.log

# deletion free blocks (2), last and second blocks are deleted (in reverse order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-free-blocks coli >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content coli >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony coli >> out$Env:UNITTEST_NUM.log

# insertion 20 ints from the file, 3 indexes from the list of free indexes (idx = 7, 9, 8) and allocation of 4 blocks
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-int-from-file coli int.txt >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content coli >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony coli >> out$Env:UNITTEST_NUM.log

# deletion colony
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-colony coli >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content coli >> out$Env:UNITTEST_NUM.log

# creation colony of PMEMoids (block capacity = 7)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile create-colony colp PMEMoid 7 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log

# insertion 20 PMEMoids from the file, allocation of 3 blocks
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid-from-file colp pmemoid.txt >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony colp >> out$Env:UNITTEST_NUM.log

# 2x removal range, indexes go to the list of free indexes, second and first blocks are freed (in this order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 6 15 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 0 5 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony colp >> out$Env:UNITTEST_NUM.log

# deletion free blocks (2), second and first blocks are deleted (in reverse order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-free-blocks colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony colp >> out$Env:UNITTEST_NUM.log

# insertion PMEMoids (20 from file and 4 single), 2 indexes from the list of free indexes, 1 index as unfilled before,
# allocation of 3 blocks
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid-from-file colp pmemoid.txt >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid colp 40731435551616 985633251211 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid colp 35551698674407316 14073705021046740 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid colp 98674447107335551 777441113705002 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid colp 5858755551616 425370962235 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony colp >> out$Env:UNITTEST_NUM.log

# removal range, one before last and last blocks are freed (in this order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 13 27 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony colp >> out$Env:UNITTEST_NUM.log

# deletion free blocks (2), one before last and last blocks are deleted (in reverse order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-free-blocks colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony colp >> out$Env:UNITTEST_NUM.log

# removal range, indexes go to the list of free indexes, first block is freed
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 0 12 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony colp >> out$Env:UNITTEST_NUM.log

# insertion 20 PMEMoids from the file, 2 blocks are taken from the list of free blocks
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid-from-file colp pmemoid.txt >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile insert-PMEMoid colp 12596521616 40000900235 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony colp >> out$Env:UNITTEST_NUM.log

# 2x removal range, indexes go to the list of free indexes, first and last blocks are freed (in this order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 0 9 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile remove-range colp 13 20 >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony colp >> out$Env:UNITTEST_NUM.log

# deletion free blocks (2), first and last blocks are deleted (in reverse order)
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-free-blocks colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-colony colp >> out$Env:UNITTEST_NUM.log

# deletion colony
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile delete-colony colp >> out$Env:UNITTEST_NUM.log
expect_normal_exit $Env:EXAMPLES_DIR\ex_pmemobj_colony $DIR\testfile print-content colp >> out$Env:UNITTEST_NUM.log

Remove-Item int.txt -Force
Remove-Item pmemoid.txt -Force
Remove-Item file_with_changing_permissions -Force
Remove-Item file_without_permissions -Force

check

pass
