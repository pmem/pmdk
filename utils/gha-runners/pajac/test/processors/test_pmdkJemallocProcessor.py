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

from junitxml import testsuite, result
from processors import pmdkJemallocProcessor


class TestJemallocProcessor(unittest.TestCase):
    def setUp(self):
        self.temp = testsuite.test_suite_count
        testsuite.test_suite_count = 0

    def tearDown(self):
        testsuite.test_suite_count = self.temp

    def test_only_passed(self):
        example_input = r'''make[3]: Nothing to be done for 'tests'.
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/jemalloc'
make objroot=../nondebug/libvmem/jemalloc/ -f ../nondebug/libvmem/jemalloc/Makefile -C /home/jenkins-slave/workspace/PMDK-unittests/src/jemalloc check
make[3]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/jemalloc'
/bin/sh ../nondebug/libvmem/jemalloc/test/test.sh ../nondebug/libvmem/jemalloc/test/unit/bitmap ../nondebug/libvmem/jemalloc/test/unit/ckh ../nondebug/libvmem/jemalloc/test/unit/hash ../nondebug/libvmem/jemalloc/test/unit/junk ../nondebug/libvmem/jemalloc/test/unit/mallctl ../nondebug/libvmem/jemalloc/test/unit/math ../nondebug/libvmem/jemalloc/test/unit/mq ../nondebug/libvmem/jemalloc/test/unit/mtx ../nondebug/libvmem/jemalloc/test/unit/prof_accum ../nondebug/libvmem/jemalloc/test/unit/prof_gdump ../nondebug/libvmem/jemalloc/test/unit/prof_idump ../nondebug/libvmem/jemalloc/test/unit/ql ../nondebug/libvmem/jemalloc/test/unit/qr ../nondebug/libvmem/jemalloc/test/unit/quarantine ../nondebug/libvmem/jemalloc/test/unit/rb ../nondebug/libvmem/jemalloc/test/unit/rtree ../nondebug/libvmem/jemalloc/test/unit/SFMT ../nondebug/libvmem/jemalloc/test/unit/stats ../nondebug/libvmem/jemalloc/test/unit/tsd ../nondebug/libvmem/jemalloc/test/unit/util ../nondebug/libvmem/jemalloc/test/unit/zero ../nondebug/libvmem/jemalloc/test/unit/pool_base_alloc ../nondebug/libvmem/jemalloc/test/unit/pool_custom_alloc ../nondebug/libvmem/jemalloc/test/unit/pool_custom_alloc_internal ../nondebug/libvmem/jemalloc/test/integration/aligned_alloc ../nondebug/libvmem/jemalloc/test/integration/allocated ../nondebug/libvmem/jemalloc/test/integration/mallocx ../nondebug/libvmem/jemalloc/test/integration/MALLOCX_ARENA ../nondebug/libvmem/jemalloc/test/integration/posix_memalign ../nondebug/libvmem/jemalloc/test/integration/rallocx ../nondebug/libvmem/jemalloc/test/integration/thread_arena ../nondebug/libvmem/jemalloc/test/integration/thread_tcache_enabled ../nondebug/libvmem/jemalloc/test/integration/xallocx ../nondebug/libvmem/jemalloc/test/integration/chunk
=== ../nondebug/libvmem/jemalloc/test/unit/bitmap ===
test_bitmap_size: pass
test_bitmap_init: pass
test_bitmap_set: pass
test_bitmap_unset: pass
test_bitmap_sfu: pass
--- pass: 5/5, skip: 0/5, fail: 0/5 ---

=== ../nondebug/libvmem/jemalloc/test/unit/ckh ===
test_new_delete: pass
'''
        test_cases = pmdkJemallocProcessor.process(example_input)

        skipped = sum(isinstance(x.result, result.Skipped) for x in test_cases)
        errors = sum(isinstance(x.result, result.Error) for x in test_cases)
        failures = sum(isinstance(x.result, result.Failure) for x in test_cases)

        self.assertEqual(len(test_cases), 5)
        self.assertEqual(skipped, 0)
        self.assertEqual(errors, 0)
        self.assertEqual(failures, 0)

        self.assertEqual(test_cases[0].name, "test_bitmap_size")
        self.assertEqual(test_cases[1].name, "test_bitmap_init")
        self.assertEqual(test_cases[2].name, "test_bitmap_set")
        self.assertEqual(test_cases[3].name, "test_bitmap_unset")
        self.assertEqual(test_cases[4].name, "test_bitmap_sfu")

        for tc in test_cases:
            self.assertEqual(tc.classname, "jemallocTests./nondebug/libvmem/jemalloc/test/unit/bitmap")

    def test_passed_skipped(self):
        example_input = r'''
=== ../nondebug/libvmem/jemalloc/test/unit/mtx ===
test_mtx_basic: pass
test_mtx_race: pass
--- pass: 2/2, skip: 0/2, fail: 0/2 ---

=== ../nondebug/libvmem/jemalloc/test/unit/prof_accum ===
test_idump:/home/jenkins-slave/workspace/PMDK-unittests/src/jemalloc/test/unit/prof_accum.c:63: Test skipped: (!config_prof)
test_idump: skip
--- pass: 0/1, skip: 1/1, fail: 0/1 ---

=== ../nondebug/libvmem/jemalloc/test/unit/prof_gdump ===
test_gdump:/home/jenkins-slave/workspace/PMDK-unittests/src/jemalloc/test/unit/prof_gdump.c:27: Test skipped: (!config_prof)
'''
        test_cases = pmdkJemallocProcessor.process(example_input)

        skipped = sum(isinstance(x.result, result.Skipped) for x in test_cases)
        errors = sum(isinstance(x.result, result.Error) for x in test_cases)
        failures = sum(isinstance(x.result, result.Failure) for x in test_cases)

        self.assertEqual(len(test_cases), 3)
        self.assertEqual(skipped, 1)
        self.assertEqual(errors, 0)
        self.assertEqual(failures, 0)

        self.assertEqual(test_cases[0].name, "test_mtx_basic")
        self.assertEqual(test_cases[1].name, "test_mtx_race")
        self.assertEqual(test_cases[2].name, "test_idump")

        self.assertIsInstance(test_cases[2].result, result.Skipped)

        self.assertEqual(test_cases[0].classname, "jemallocTests./nondebug/libvmem/jemalloc/test/unit/mtx")
        self.assertEqual(test_cases[1].classname, "jemallocTests./nondebug/libvmem/jemalloc/test/unit/mtx")
        self.assertEqual(test_cases[2].classname, "jemallocTests./nondebug/libvmem/jemalloc/test/unit/prof_accum")


