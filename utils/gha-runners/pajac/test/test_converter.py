#
# Copyright 2019-2020, Intel Corporation
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

import unittest
import os
import time
from processors import process
from preprocess.preprocess import preprocess



class TestConverter(unittest.TestCase):

    def process(self, input_string: str):
        preprocessed_input_string = preprocess(input_string)
        return process.to_test_suites(preprocessed_input_string)

    def run_conversion_on_file_and_save(self, testfile_name, xmlfile_name):
        with open(testfile_name, 'r', encoding='utf-8') as input_file:
            input_string = input_file.read()

        test_suites = self.process(input_string)
        output_string = process.from_test_suites_to_xml(test_suites)

        with open(xmlfile_name, 'w', encoding='utf-8') as output_file:
            output_file.write(output_string)

        return test_suites

    def test_real_results(self):
        with open('test/test_0.log', 'r', encoding='utf-8') as input_file:
            input_string = input_file.read()

        test_suites = self.process(input_string)
        passed = 1845
        skipped = 134
        failed = 0
        errors = 0

        self.assertEqual(len(test_suites), 1)
        self.assertEqual(test_suites[0].tests, passed + skipped + failed + errors)
        self.assertEqual(test_suites[0].skipped, skipped)
        self.assertEqual(test_suites[0].failures, failed)
        self.assertEqual(test_suites[0].errors, errors)

    def test_windows_results(self):
        with open('test/test_windows.log', 'r', encoding='utf-8') as input_file:
            input_string = input_file.read()

        test_suites = self.process(input_string)
        passed = 502
        skipped = 36
        failed = 0
        errors = 1

        self.assertEqual(len(test_suites), 1)
        self.assertEqual(test_suites[0].tests, passed + skipped + failed + errors)
        self.assertEqual(test_suites[0].skipped, skipped)
        self.assertEqual(test_suites[0].failures, failed)
        self.assertEqual(test_suites[0].errors, errors)


    def test_real_python_results(self):
        with open('test/test_python.log', 'r', encoding='utf-8') as input_file:
            input_string = input_file.read()

        test_suites = self.process(input_string)
        passed = 38
        skipped = 21
        failed = 2
        errors = 0

        self.assertEqual(len(test_suites), 1)
        self.assertEqual(test_suites[0].errors, errors)
        self.assertEqual(test_suites[0].skipped, skipped)
        self.assertEqual(test_suites[0].failures, failed)
        self.assertEqual(test_suites[0].tests, passed + skipped + failed + errors)



    def test_performance(self):
        passed_test_entry = "blk_nblock/TEST0: SETUP (check/pmem/nondebug)\nblk_nblock/TEST0: PASS			[01.237 s]\n"
        skipped_test_entry = "ctl_prefault/TEST0: SETUP (check/non-pmem/nondebug)\nctl_prefault/TEST0: SKIP: filesystem does not support fallocate\n"
        error_test_entry = "ctl_cow/TEST3: SETUP (check/non-pmem/static-debug)\n"
        failed_test_entry = """obj_tx_strdup/TEST0: SETUP (all/pmem/debug/pmemcheck)
obj_tx_strdup/TEST0 failed with Valgrind. See pmemcheck0.log. Last 20 lines below.
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x4C6DB10: alloc_prep_block (palloc.c:154)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x4C6DE3F: palloc_reservation_create (palloc.c:242)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x4C6E9C1: palloc_reserve (palloc.c:609)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x4C763D2: tx_alloc_common (tx.c:613)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x4C799F1: pmemobj_tx_xstrdup (tx.c:1700)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x403F6C: do_tx_strdup_noflush (obj_tx_strdup.c:408)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x404173: main (obj_tx_strdup.c:443)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758== 	Address: 0x75c0b50	size: 14	state: DIRTY
obj_tx_strdup/TEST0 pmemcheck0.log ==6758== [1]    at 0x5AE7811: __memcpy_avx_unaligned_erms (in /lib64/libc-2.26.so)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x4C7555D: constructor_tx_alloc (tx.c:257)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x4C6DB10: alloc_prep_block (palloc.c:154)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x4C6DE3F: palloc_reservation_create (palloc.c:242)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x4C6E9C1: palloc_reserve (palloc.c:609)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x4C763D2: tx_alloc_common (tx.c:613)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x4C79CD8: pmemobj_tx_xwcsdup (tx.c:1756)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x403F80: do_tx_strdup_noflush (obj_tx_strdup.c:410)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758==    by 0x404173: main (obj_tx_strdup.c:443)
obj_tx_strdup/TEST0 pmemcheck0.log ==6758== 	Address: 0x75c0bd0	size: 56	state: DIRTY
obj_tx_strdup/TEST0 pmemcheck0.log ==6758== Total memory not made persistent: 70
obj_tx_strdup/TEST0 pmemcheck0.log ==6758== ERROR SUMMARY: 2 errors
RUNTESTS: stopping: obj_tx_strdup//TEST0 failed, TEST=all FS=pmem BUILD=debug
"""

        passed_tests_count = 2000
        skipped_tests_count = 300
        failed_tests_count = 150
        error_tests_count = 150

        testfile_name = "generated_test.log"
        xmlfile_name = "generated_test.xml"

        # remove test files if ever existed:
        try:
            os.remove(testfile_name)
        except OSError:
            pass

        try:
            os.remove(xmlfile_name)
        except OSError:
            pass

        # generate file with artificial tests results content:
        with open(testfile_name, "w+") as file:
            for i in range(0, passed_tests_count):
                file.write(passed_test_entry)

            for i in range(0, skipped_tests_count):
                file.write(skipped_test_entry)

            for i in range(0, failed_tests_count):
                file.write(failed_test_entry)

            for i in range(0, error_tests_count):
                file.write(error_test_entry)

        # get current time as the start time of the conversion
        # (we measure loading test file, converting to testcases, converting to XML and saving the result):
        start_time = time.time()

        # run the conversion:
        test_suites = self.run_conversion_on_file_and_save(testfile_name, xmlfile_name)

        # get current time as the end time of the conversion:
        end_time = time.time()

        # check correctness of parsed results:
        self.assertEqual(len(test_suites), 1)
        self.assertEqual(test_suites[0].errors, error_tests_count)
        self.assertEqual(test_suites[0].skipped, skipped_tests_count)
        self.assertEqual(test_suites[0].failures, failed_tests_count)
        self.assertEqual(test_suites[0].tests, passed_tests_count + skipped_tests_count + failed_tests_count + error_tests_count)

        # check time constrains - please treat that as a hint only but constrains should be satisfied on the average developer machine:
        duration = end_time - start_time
        maximum_expected_seconds = 20
        # when this check fails this is a suggestion for reconsidering implemented algorithms:
        self.assertEqual(duration < maximum_expected_seconds, True, msg=
            "WARNING! Conversion took " + str(duration) +
            " seconds while the estimated maximum time is " + str(maximum_expected_seconds) +
            " seconds. Defined set of tests should have be converted faster, but this check should be treated only as" +
            " a hint and suggestion for reconsidering implemented algorithms - performance is heavily hardware dependent" +
            " and there is no fixed set of hardware to run this test on.")

        # remove created files:
        try:
            os.remove(testfile_name)
        except OSError:
            pass

        try:
            os.remove(xmlfile_name)
        except OSError:
            pass
