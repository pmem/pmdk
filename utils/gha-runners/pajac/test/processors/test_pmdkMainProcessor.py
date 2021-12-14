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
from processors import pmdkMainProcessor


class TestMainProcessor(unittest.TestCase):
    def setUp(self):
        self.temp = testsuite.test_suite_count
        testsuite.test_suite_count = 0

    def tearDown(self):
        testsuite.test_suite_count = self.temp

    def test_only_passed(self):
        example_input = r'''make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/remote_obj_basic'
make -C pmempool_info_remote sync-test
make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_info_remote'
../sync-remotes/copy-to-remote-nodes.sh test
make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_info_remote'
make -C pmempool_feature_remote sync-test
make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_feature_remote'
../sync-remotes/copy-to-remote-nodes.sh test
make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_feature_remote'
make -C pmempool_rm_remote sync-test
make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_rm_remote'
../sync-remotes/copy-to-remote-nodes.sh test
make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_rm_remote'
make -C pmempool_sync_remote sync-test
make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_sync_remote'
../sync-remotes/copy-to-remote-nodes.sh test
make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_sync_remote'
make -C pmempool_transform_remote sync-test
make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_transform_remote'
../sync-remotes/copy-to-remote-nodes.sh test
make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_transform_remote'
remote_obj_basic/TEST2: SETUP (medium/non-pmem/debug/memcheck)
remote_obj_basic/TEST2: PASS			[02.950 s]
remote_obj_basic/TEST3: SETUP (medium/pmem/debug/memcheck)
remote_obj_basic/TEST3: PASS			[03.493 s]
remote_obj_basic/TEST3: SETUP (medium/non-pmem/debug/memcheck)
remote_obj_basic/TEST3: PASS			[03.554 s]
pmempool_info_remote/TEST0: SETUP (medium/pmem/debug/memcheck/sockets/GPSPM)
libi40iw-i40iw_vmapped_qp: failed to pin memory for SQ
libi40iw-i40iw_ucreate_qp: failed to map QP
libi40iw-i40iw_vmapped_qp: failed to pin memory for SQ
libi40iw-i40iw_ucreate_qp: failed to map QP
libi40iw-i40iw_vmapped_qp: failed to pin memory for SQ
libi40iw-i40iw_ucreate_qp: failed to map QP
libi40iw-i40iw_vmapped_qp: failed to pin memory for SQ
libi40iw-i40iw_ucreate_qp: failed to map QP
pmempool_info_remote/TEST0: PASS			[07.871 s]'''

        test_cases = pmdkMainProcessor.process(example_input)

        skipped = sum(isinstance(x.result, result.Skipped) for x in test_cases)
        errors = sum(isinstance(x.result, result.Error) for x in test_cases)
        failures = sum(isinstance(x.result, result.Failure) for x in test_cases)

        self.assertEqual(len(test_cases), 4)
        self.assertEqual(skipped, 0)
        self.assertEqual(errors, 0)
        self.assertEqual(failures, 0)

        self.assertEqual(test_cases[0].stdout, """[line 18] make -C pmempool_transform_remote sync-test
[line 19] make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_transform_remote'
[line 20] ../sync-remotes/copy-to-remote-nodes.sh test
[line 21] make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_transform_remote'
[line 22] remote_obj_basic/TEST2: SETUP (medium/non-pmem/debug/memcheck)
[==line 23==] remote_obj_basic/TEST2: PASS			[02.950 s]""")

        self.assertEqual(test_cases[1].stdout, """[line 20] ../sync-remotes/copy-to-remote-nodes.sh test
[line 21] make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_transform_remote'
[line 22] remote_obj_basic/TEST2: SETUP (medium/non-pmem/debug/memcheck)
[line 23] remote_obj_basic/TEST2: PASS			[02.950 s]
[line 24] remote_obj_basic/TEST3: SETUP (medium/pmem/debug/memcheck)
[==line 25==] remote_obj_basic/TEST3: PASS			[03.493 s]""")

        self.assertEqual(test_cases[2].stdout, """[line 22] remote_obj_basic/TEST2: SETUP (medium/non-pmem/debug/memcheck)
[line 23] remote_obj_basic/TEST2: PASS			[02.950 s]
[line 24] remote_obj_basic/TEST3: SETUP (medium/pmem/debug/memcheck)
[line 25] remote_obj_basic/TEST3: PASS			[03.493 s]
[line 26] remote_obj_basic/TEST3: SETUP (medium/non-pmem/debug/memcheck)
[==line 27==] remote_obj_basic/TEST3: PASS			[03.554 s]""")

        self.assertEqual(test_cases[3].stdout, """[line 32] libi40iw-i40iw_ucreate_qp: failed to map QP
[line 33] libi40iw-i40iw_vmapped_qp: failed to pin memory for SQ
[line 34] libi40iw-i40iw_ucreate_qp: failed to map QP
[line 35] libi40iw-i40iw_vmapped_qp: failed to pin memory for SQ
[line 36] libi40iw-i40iw_ucreate_qp: failed to map QP
[==line 37==] pmempool_info_remote/TEST0: PASS			[07.871 s]""")

        self.assertEqual(test_cases[0].name, "TEST2")
        self.assertEqual(test_cases[1].name, "TEST3")
        self.assertEqual(test_cases[2].name, "TEST3")
        self.assertEqual(test_cases[3].name, "TEST0")

        self.assertEqual(test_cases[0].classname, "pmdkUnitTests.remote_obj_basic")
        self.assertEqual(test_cases[1].classname, "pmdkUnitTests.remote_obj_basic")
        self.assertEqual(test_cases[2].classname, "pmdkUnitTests.remote_obj_basic")
        self.assertEqual(test_cases[3].classname, "pmdkUnitTests.pmempool_info_remote")

    def test_only_skipped(self):
        example_input = r'''make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_rm_remote'
make -C pmempool_sync_remote sync-test
obj_rpmem_basic_integration/TEST9: SKIP: remote valgrind tests disabled
obj_rpmem_basic_integration/TEST10: SKIP: remote valgrind tests disabled
obj_rpmem_basic_integration/TEST11: SKIP: remote valgrind tests disabled
make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/remote_obj_basic'
make -C pmempool_info_remote sync-test
make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_info_remote'
../sync-remotes/copy-to-remote-nodes.sh test
make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_info_remote'
make -C pmempool_feature_remote sync-test
make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_feature_remote'
../sync-remotes/copy-to-remote-nodes.sh test
make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_feature_remote'
make -C pmempool_rm_remote sync-test
make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_rm_remote'
../sync-remotes/copy-to-remote-nodes.sh test
make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_rm_remote'
make -C pmempool_sync_remote sync-test'''

        test_cases = pmdkMainProcessor.process(example_input)

        skipped = sum(isinstance(x.result, result.Skipped) for x in test_cases)
        errors = sum(isinstance(x.result, result.Error) for x in test_cases)
        failures = sum(isinstance(x.result, result.Failure) for x in test_cases)

        self.assertEqual(len(test_cases), 3)
        self.assertEqual(skipped, 3)
        self.assertEqual(errors, 0)
        self.assertEqual(failures, 0)

        self.assertEqual(test_cases[0].name, "TEST9")
        self.assertEqual(test_cases[1].name, "TEST10")
        self.assertEqual(test_cases[2].name, "TEST11")

        self.assertEqual(test_cases[0].stdout, """[line 1] make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_rm_remote'
[line 2] make -C pmempool_sync_remote sync-test
[==line 3==] obj_rpmem_basic_integration/TEST9: SKIP: remote valgrind tests disabled""")

        self.assertEqual(test_cases[1].stdout, """[line 1] make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_rm_remote'
[line 2] make -C pmempool_sync_remote sync-test
[line 3] obj_rpmem_basic_integration/TEST9: SKIP: remote valgrind tests disabled
[==line 4==] obj_rpmem_basic_integration/TEST10: SKIP: remote valgrind tests disabled""")

        self.assertEqual(test_cases[2].stdout, """[line 1] make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_rm_remote'
[line 2] make -C pmempool_sync_remote sync-test
[line 3] obj_rpmem_basic_integration/TEST9: SKIP: remote valgrind tests disabled
[line 4] obj_rpmem_basic_integration/TEST10: SKIP: remote valgrind tests disabled
[==line 5==] obj_rpmem_basic_integration/TEST11: SKIP: remote valgrind tests disabled""")

        for tc in test_cases:
            self.assertEqual(tc.classname, "pmdkUnitTests.obj_rpmem_basic_integration")
            self.assertIsInstance(tc.result, result.Skipped)
            self.assertEqual(tc.result.message, "SKIP: remote valgrind tests disabled")

    def test_only_failure(self):

        failure_input = r'''obj_fragmentation2/TEST8: SETUP (long/pmem/static-nondebug/pmemcheck)
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_list_insert'
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_list_recovery'
make -C obj_memcheck_register pcheck
make -C obj_memops pcheck
make[3]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_memcheck_register'
make[3]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_memops'
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_list_move'
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_list_remove'
make -C obj_oid_thread pcheck
make[3]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_oid_thread'
make -C obj_out_of_memory pcheck
make[3]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_out_of_memory'
make -C obj_persist_count pcheck
make -C obj_pmalloc_basic pcheck
make[3]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_persist_count'
make[3]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_pmalloc_basic'
make -C obj_pmalloc_mt pcheck
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_locks'
make -C obj_pmalloc_oom_mt pcheck
make[3]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_pmalloc_mt'
make -C obj_pmalloc_rand_mt pcheck
make -C obj_pmemcheck pcheck
make[3]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_pmalloc_rand_mt'
make[3]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_pmalloc_oom_mt'
make[3]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_pmemcheck'
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_memcheck'
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_memblock'
make -C obj_pool pcheck
make[3]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_pool'
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_list_macro'
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_mem'
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_memcheck_register'
make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/rpmem_obc'
obj_fragmentation2/TEST8 failed with Valgrind. See pmemcheck8.log. Last 20 lines below.
obj_fragmentation2/TEST8 pmemcheck8.log ==104662== 	Address: 0x13ff82408	size: 8	state: DIRTY
obj_fragmentation2/TEST8 pmemcheck8.log ==104662== [81262]    at 0x42BAF0: ulog_entry_apply (in /home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_fragmentation2/obj_fragmentation2.static-nondebug)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x41E848: operation_process (in /home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_fragmentation2/obj_fragmentation2.static-nondebug)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x423A2B: palloc_exec_actions (in /home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_fragmentation2/obj_fragmentation2.static-nondebug)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x423F61: palloc_operation (in /home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_fragmentation2/obj_fragmentation2.static-nondebug)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x41FF23: obj_alloc_construct (in /home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_fragmentation2/obj_fragmentation2.static-nondebug)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x421DBA: pmemobj_alloc (in /home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_fragmentation2/obj_fragmentation2.static-nondebug)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x405698: main (obj_fragmentation2.c:232)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662== 	Address: 0x13ff82410	size: 256	state: DIRTY
obj_fragmentation2/TEST8 pmemcheck8.log ==104662== [81263]    at 0x55CDD15: __memset_avx2_unaligned_erms (in /lib64/libc-2.26.so)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x41D394: memblock_run_init (in /home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_fragmentation2/obj_fragmentation2.static-nondebug)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x417BFF: heap_get_bestfit_block (in /home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_fragmentation2/obj_fragmentation2.static-nondebug)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x423692: palloc_reservation_create (in /home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_fragmentation2/obj_fragmentation2.static-nondebug)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x424039: palloc_operation (in /home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_fragmentation2/obj_fragmentation2.static-nondebug)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x41FF23: obj_alloc_construct (in /home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_fragmentation2/obj_fragmentation2.static-nondebug)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x421DBA: pmemobj_alloc (in /home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_fragmentation2/obj_fragmentation2.static-nondebug)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662==    by 0x405698: main (obj_fragmentation2.c:232)
obj_fragmentation2/TEST8 pmemcheck8.log ==104662== 	Address: 0x13ff82510	size: 48	state: DIRTY
obj_fragmentation2/TEST8 pmemcheck8.log ==104662== Total memory not made persistent: 5929008
obj_fragmentation2/TEST8 pmemcheck8.log ==104662== ERROR SUMMARY: 81264 errors
RUNTESTS: stopping: obj_fragmentation2/TEST8 failed, TEST=long FS=pmem BUILD=static-nondebug
'''

        before_failure_input = r'''make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/remote_obj_basic'
make -C pmempool_info_remote sync-test
make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_info_remote'
../sync-remotes/copy-to-remote-nodes.sh test
make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_info_remote'
make -C pmempool_feature_remote sync-test
make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_feature_remote'
../sync-remotes/copy-to-remote-nodes.sh test
make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_feature_remote'
make -C pmempool_rm_remote sync-test
make[4]: Entering directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_rm_remote'
../sync-remotes/copy-to-remote-nodes.sh test
make[4]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/pmempool_rm_remote'
make -C pmempool_sync_remote sync-test
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_layout'''

        after_failure_input = r'''make[3]: *** Waiting for unfinished jobs....
        '''

        example_input = before_failure_input + "\n" + failure_input + "\n" + after_failure_input

        test_cases = pmdkMainProcessor.process(example_input)

        skipped = sum(isinstance(x.result, result.Skipped) for x in test_cases)
        errors = sum(isinstance(x.result, result.Error) for x in test_cases)
        failures = sum(isinstance(x.result, result.Failure) for x in test_cases)

        self.assertEqual(len(test_cases), 1)
        self.assertEqual(skipped, 0)
        self.assertEqual(errors, 0)
        self.assertEqual(failures, 1)

        self.assertEqual(test_cases[0].name, "TEST8")
        self.assertEqual(test_cases[0].classname, "pmdkUnitTests.obj_fragmentation2")

        for fail_input_line in failure_input.splitlines():
            self.assertIn(fail_input_line, test_cases[0].stdout)

        self.assertIsInstance(test_cases[0].result, result.Failure)
        self.assertEqual(test_cases[0].result.message,
                         "RUNTESTS: stopping: obj_fragmentation2/TEST8 failed, TEST=long FS=pmem BUILD=static-nondebug")

    def test_fail_by_timeout(self):
        example_input = r'''obj_sync/TEST2: SETUP (check/none/debug/helgrind)
obj_sync/TEST2: PASS			[00.500 s]
obj_sync/TEST2: SETUP (check/none/nondebug/helgrind)
RUNTESTS: stopping: obj_sync//TEST2 timed out, TEST=check FS=none BUILD=nondebug
obj_sync/TEST3: SETUP (check/none/debug/drd)
obj_sync/TEST3: PASS			[00.769 s]'''
        test_cases = pmdkMainProcessor.process(example_input)

        skipped = sum(isinstance(x.result, result.Skipped) for x in test_cases)
        errors = sum(isinstance(x.result, result.Error) for x in test_cases)
        failures = sum(isinstance(x.result, result.Failure) for x in test_cases)

        self.assertEqual(len(test_cases), 3)
        self.assertEqual(failures, 1)
        self.assertEqual(skipped, 0)
        self.assertEqual(errors, 0)

        for tc in test_cases:
            self.assertEqual(tc.classname, "pmdkUnitTests.obj_sync")

        self.assertEqual(test_cases[2].name, "TEST2")

        self.assertIsInstance(test_cases[2].result, result.Failure)

        self.assertEqual(test_cases[2].result.message,
                         "RUNTESTS: stopping: obj_sync//TEST2 timed out, TEST=check FS=none BUILD=nondebug")

        self.assertEqual(test_cases[2].stdout, r'''--- [approach: nearest matching SETUP] test trace for: obj_sync/TEST2 TEST=check FS=none BUILD=nondebug ---
[line 3] obj_sync/TEST2: SETUP (check/none/nondebug/helgrind)
[==line 4==] RUNTESTS: stopping: obj_sync//TEST2 timed out, TEST=check FS=none BUILD=nondebug
''')

        self.assertEqual(test_cases[0].name, "TEST2")
        self.assertIsInstance(test_cases[0].result, result.Passed)

        self.assertEqual(test_cases[1].name, "TEST3")
        self.assertIsInstance(test_cases[1].result, result.Passed)

    def test_fail_with_double_slash(self):
        example_input = r'''obj_tx_add_range/TEST0: SETUP (all/pmem/debug/memcheck)
obj_tx_add_range/TEST0 failed with Valgrind. See memcheck0.log. Last 20 lines below.
yes: standard output: Broken pipe
obj_tx_add_range/TEST0 memcheck0.log ==55432==  Address 0x57bb618 is 8 bytes inside a block of size 1,072 client-defined
obj_tx_add_range/TEST0 memcheck0.log ==55432==    at 0x4888C4B: alloc_prep_block (palloc.c:137)
obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x48891F4: palloc_reservation_create (palloc.c:242)
obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x4889D15: palloc_reserve (palloc.c:603)
obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x48916D1: tx_alloc_common (tx.c:595)
obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x4894098: pmemobj_tx_alloc (tx.c:1486)
obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x406400: do_tx_alloc (obj_tx_add_range.c:108)
obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x409AA6: do_tx_add_range_no_uninit_check_commit_no_flag (obj_tx_add_range.c:744)
obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x40C3CB: main (obj_tx_add_range.c:1127)
obj_tx_add_range/TEST0 memcheck0.log ==55432== 
obj_tx_add_range/TEST0 memcheck0.log ==55432== 
obj_tx_add_range/TEST0 memcheck0.log ==55432== HEAP SUMMARY:
obj_tx_add_range/TEST0 memcheck0.log ==55432==     in use at exit: 0 bytes in 0 blocks
obj_tx_add_range/TEST0 memcheck0.log ==55432==   total heap usage: 26,625 allocs, 26,594 frees, 54,251,170 bytes allocated
obj_tx_add_range/TEST0 memcheck0.log ==55432== 
obj_tx_add_range/TEST0 memcheck0.log ==55432== All heap blocks were freed -- no leaks are possible
obj_tx_add_range/TEST0 memcheck0.log ==55432== 
obj_tx_add_range/TEST0 memcheck0.log ==55432== Use --track-origins=yes to see where uninitialised values come from
obj_tx_add_range/TEST0 memcheck0.log ==55432== For lists of detected and suppressed errors, rerun with: -s
obj_tx_add_range/TEST0 memcheck0.log ==55432== ERROR SUMMARY: 1 errors from 1 contexts (suppressed: 0 from 0)
RUNTESTS: stopping: obj_tx_add_range//TEST0 failed, TEST=all FS=pmem BUILD=debug
obj_tx_add_range/TEST1: SKIP RUNTESTS script parameter memcheck tries to enable different valgrind test than one defined in TEST
'''
        test_cases = pmdkMainProcessor.process(example_input)

        skipped = sum(isinstance(x.result, result.Skipped) for x in test_cases)
        errors = sum(isinstance(x.result, result.Error) for x in test_cases)
        failures = sum(isinstance(x.result, result.Failure) for x in test_cases)

        self.assertEqual(len(test_cases), 2)
        self.assertEqual(skipped, 1)
        self.assertEqual(errors, 0)
        self.assertEqual(failures, 1)

        # expected "TEST1" first, because of parsing order: pass + skip first, then failures, then errors
        self.assertEqual(test_cases[0].name, "TEST1")
        self.assertEqual(test_cases[1].name, "TEST0")

        for tc in test_cases:
            self.assertEqual(tc.classname, "pmdkUnitTests.obj_tx_add_range")

        self.assertIsInstance(test_cases[0].result, result.Skipped)
        self.assertIsInstance(test_cases[1].result, result.Failure)

        self.assertEqual(test_cases[0].result.message, "SKIP RUNTESTS script parameter memcheck tries to enable different valgrind test than one defined in TEST")

        self.assertEqual(test_cases[1].result.message, "RUNTESTS: stopping: obj_tx_add_range//TEST0 failed, TEST=all FS=pmem BUILD=debug")

        self.assertEqual(test_cases[0].stdout, r'''[line 20] obj_tx_add_range/TEST0 memcheck0.log ==55432== 
[line 21] obj_tx_add_range/TEST0 memcheck0.log ==55432== Use --track-origins=yes to see where uninitialised values come from
[line 22] obj_tx_add_range/TEST0 memcheck0.log ==55432== For lists of detected and suppressed errors, rerun with: -s
[line 23] obj_tx_add_range/TEST0 memcheck0.log ==55432== ERROR SUMMARY: 1 errors from 1 contexts (suppressed: 0 from 0)
[line 24] RUNTESTS: stopping: obj_tx_add_range//TEST0 failed, TEST=all FS=pmem BUILD=debug
[==line 25==] obj_tx_add_range/TEST1: SKIP RUNTESTS script parameter memcheck tries to enable different valgrind test than one defined in TEST''')

        self.assertEqual(test_cases[1].stdout, r'''--- [approach: nearest matching SETUP] test trace for: obj_tx_add_range/TEST0 TEST=all FS=pmem BUILD=debug ---
[line 1] obj_tx_add_range/TEST0: SETUP (all/pmem/debug/memcheck)
[line 2] obj_tx_add_range/TEST0 failed with Valgrind. See memcheck0.log. Last 20 lines below.
[line 3] yes: standard output: Broken pipe
[line 4] obj_tx_add_range/TEST0 memcheck0.log ==55432==  Address 0x57bb618 is 8 bytes inside a block of size 1,072 client-defined
[line 5] obj_tx_add_range/TEST0 memcheck0.log ==55432==    at 0x4888C4B: alloc_prep_block (palloc.c:137)
[line 6] obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x48891F4: palloc_reservation_create (palloc.c:242)
[line 7] obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x4889D15: palloc_reserve (palloc.c:603)
[line 8] obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x48916D1: tx_alloc_common (tx.c:595)
[line 9] obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x4894098: pmemobj_tx_alloc (tx.c:1486)
[line 10] obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x406400: do_tx_alloc (obj_tx_add_range.c:108)
[line 11] obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x409AA6: do_tx_add_range_no_uninit_check_commit_no_flag (obj_tx_add_range.c:744)
[line 12] obj_tx_add_range/TEST0 memcheck0.log ==55432==    by 0x40C3CB: main (obj_tx_add_range.c:1127)
[line 13] obj_tx_add_range/TEST0 memcheck0.log ==55432== 
[line 14] obj_tx_add_range/TEST0 memcheck0.log ==55432== 
[line 15] obj_tx_add_range/TEST0 memcheck0.log ==55432== HEAP SUMMARY:
[line 16] obj_tx_add_range/TEST0 memcheck0.log ==55432==     in use at exit: 0 bytes in 0 blocks
[line 17] obj_tx_add_range/TEST0 memcheck0.log ==55432==   total heap usage: 26,625 allocs, 26,594 frees, 54,251,170 bytes allocated
[line 18] obj_tx_add_range/TEST0 memcheck0.log ==55432== 
[line 19] obj_tx_add_range/TEST0 memcheck0.log ==55432== All heap blocks were freed -- no leaks are possible
[line 20] obj_tx_add_range/TEST0 memcheck0.log ==55432== 
[line 21] obj_tx_add_range/TEST0 memcheck0.log ==55432== Use --track-origins=yes to see where uninitialised values come from
[line 22] obj_tx_add_range/TEST0 memcheck0.log ==55432== For lists of detected and suppressed errors, rerun with: -s
[line 23] obj_tx_add_range/TEST0 memcheck0.log ==55432== ERROR SUMMARY: 1 errors from 1 contexts (suppressed: 0 from 0)
[==line 24==] RUNTESTS: stopping: obj_tx_add_range//TEST0 failed, TEST=all FS=pmem BUILD=debug
''')

    def test_error(self):
        example_input = r'''make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_realloc'
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test'
obj_many_size_allocs/TEST0: SETUP (long/non-pmem/static-nondebug/pmemcheck)
obj_many_size_allocs/TEST2: SETUP (long/non-pmem/static-nondebug/pmemcheck)
obj_many_size_allocs/TEST2: PASS			[04:37.886 s]
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_out_of_memory'
'''

        test_cases = pmdkMainProcessor.process(example_input)

        skipped = sum(isinstance(x.result, result.Skipped) for x in test_cases)
        errors = sum(isinstance(x.result, result.Error) for x in test_cases)
        failures = sum(isinstance(x.result, result.Failure) for x in test_cases)

        self.assertEqual(len(test_cases), 2)
        self.assertEqual(skipped, 0)
        self.assertEqual(errors, 1)
        self.assertEqual(failures, 0)

        self.assertEqual(test_cases[0].name, "TEST2")
        self.assertEqual(test_cases[1].name, "TEST0")

        for tc in test_cases:
            self.assertEqual(tc.classname, "pmdkUnitTests.obj_many_size_allocs")

        self.assertIsInstance(test_cases[0].result, result.Passed)
        self.assertIsInstance(test_cases[1].result, result.Error)
        self.assertEqual(test_cases[1].result.message, "SETUP (long/non-pmem/static-nondebug/pmemcheck)")

        self.assertIn("obj_many_size_allocs/TEST0: SETUP (long/non-pmem/static-nondebug/pmemcheck)",
                      test_cases[1].stdout)

    def test_all_valgrind_tests_disabled(self):
        """ Ensure that we are ignoring printed message "all valgrind tests disabled" for tests. """

        example_input = r'''make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test/obj_realloc'
make[3]: Leaving directory '/home/jenkins-slave/workspace/PMDK-unittests/src/test'
util_badblock/TEST10: all valgrind tests disabled
util_badblock/TEST10: SETUP (check/pmem/debug)
util_badblock/TEST10: PASS			[00.049 s]
fake_group/TEST1500: all valgrind tests disabled'''

        test_cases = pmdkMainProcessor.process(example_input)
        self.assertEqual(1, len(test_cases))
        self.assertIsInstance(test_cases[0].result, result.Passed)
        self.assertEqual("TEST10", test_cases[0].name)
        self.assertEqual("pmdkUnitTests.util_badblock", test_cases[0].classname)

    def test_output_from_python_framework(self):
        example_input = r'''obj_tx_add_range_direct/TEST0: SETUP	(medium/pmem/debug/drd)
obj_tx_add_range_direct/TEST0: PASS 			[01.311 s]
obj_tx_add_range_direct/TEST0: SETUP	(medium/nonpmem/debug/drd)

Error 134
Last 9 lines of /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/drd0.log below (whole file has 9 lines):
==16915== drd, a thread error detector
==16915== Copyright (C) 2006-2017, and GNU GPL'd, by Bart Van Assche.
==16915== Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info
==16915== Command: /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/obj_tx_add_range_direct /tmp/obj_tx_add_range_direct_0/testfile0
==16915== Parent PID: 16593
==16915== 
==16915== 
==16915== For lists of detected and suppressed errors, rerun with: -s
==16915== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
Last 2 lines of /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/out0.log below (whole file has 2 lines):
obj_tx_add_range_direct/TEST0: START: obj_tx_add_range_direct
 /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/obj_tx_add_range_direct /tmp/obj_tx_add_range_direct_0/testfile0
Last 1 lines of /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/err0.log below (whole file has 1 lines):
{obj_tx_add_range_direct.c:855 main} obj_tx_add_range_direct/TEST0: Error: pmemobj_create: No such file or directory
Last 3 lines of /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/trace0.log below (whole file has 3 lines):
{obj_tx_add_range_direct.c:846 main} obj_tx_add_range_direct/TEST0: START: obj_tx_add_range_direct
 /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/obj_tx_add_range_direct /tmp/obj_tx_add_range_direct_0/testfile0
{obj_tx_add_range_direct.c:855 main} obj_tx_add_range_direct/TEST0: Error: pmemobj_create: No such file or directory
obj_tx_add_range_direct/TEST0: FAILED	(medium/nonpmem/debug/drd)
obj_tx_add_range_direct/TEST1: SKIP: test enables the 'pmemcheck' Valgrind tool while execution configuration forces 'drd' '''

        test_cases = pmdkMainProcessor.process(example_input)

        skipped = sum(isinstance(x.result, result.Skipped) for x in test_cases)
        errors = sum(isinstance(x.result, result.Error) for x in test_cases)
        failures = sum(isinstance(x.result, result.Failure) for x in test_cases)

        self.assertEqual(len(test_cases), 3)
        self.assertEqual(failures, 1)
        self.assertEqual(skipped, 1)
        self.assertEqual(errors, 0)

        for tc in test_cases:
            self.assertEqual(tc.classname, "pmdkUnitTests.obj_tx_add_range_direct")

        self.assertEqual(test_cases[0].name, "TEST0")
        self.assertEqual(test_cases[1].name, "TEST1")
        self.assertEqual(test_cases[2].name, "TEST0")

        self.assertIsInstance(test_cases[0].result, result.Passed)
        self.assertIsInstance(test_cases[1].result, result.Skipped)
        self.assertIsInstance(test_cases[2].result, result.Failure)

        self.assertEqual(test_cases[1].result.message,
                         "SKIP: test enables the 'pmemcheck' Valgrind tool while execution configuration forces 'drd' ")

        self.assertEqual(test_cases[2].result.message,
                         "obj_tx_add_range_direct/TEST0: FAILED	(medium/nonpmem/debug/drd)")

        self.assertEqual(test_cases[0].stdout, r'''[line 1] obj_tx_add_range_direct/TEST0: SETUP	(medium/pmem/debug/drd)
[==line 2==] obj_tx_add_range_direct/TEST0: PASS 			[01.311 s]''')

        self.assertEqual(test_cases[1].stdout, r'''[line 21] Last 3 lines of /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/trace0.log below (whole file has 3 lines):
[line 22] {obj_tx_add_range_direct.c:846 main} obj_tx_add_range_direct/TEST0: START: obj_tx_add_range_direct
[line 23]  /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/obj_tx_add_range_direct /tmp/obj_tx_add_range_direct_0/testfile0
[line 24] {obj_tx_add_range_direct.c:855 main} obj_tx_add_range_direct/TEST0: Error: pmemobj_create: No such file or directory
[line 25] obj_tx_add_range_direct/TEST0: FAILED	(medium/nonpmem/debug/drd)
[==line 26==] obj_tx_add_range_direct/TEST1: SKIP: test enables the 'pmemcheck' Valgrind tool while execution configuration forces 'drd' ''')

        self.assertEqual(test_cases[2].stdout, r'''--- [approach: detailed parameters for SETUP + FAIL] test trace for: obj_tx_add_range_direct/TEST0 (medium/nonpmem/debug/drd) ---
[line 3] obj_tx_add_range_direct/TEST0: SETUP	(medium/nonpmem/debug/drd)
[line 4] 
[line 5] Error 134
[line 6] Last 9 lines of /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/drd0.log below (whole file has 9 lines):
[line 7] ==16915== drd, a thread error detector
[line 8] ==16915== Copyright (C) 2006-2017, and GNU GPL'd, by Bart Van Assche.
[line 9] ==16915== Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info
[line 10] ==16915== Command: /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/obj_tx_add_range_direct /tmp/obj_tx_add_range_direct_0/testfile0
[line 11] ==16915== Parent PID: 16593
[line 12] ==16915== 
[line 13] ==16915== 
[line 14] ==16915== For lists of detected and suppressed errors, rerun with: -s
[line 15] ==16915== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
[line 16] Last 2 lines of /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/out0.log below (whole file has 2 lines):
[line 17] obj_tx_add_range_direct/TEST0: START: obj_tx_add_range_direct
[line 18]  /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/obj_tx_add_range_direct /tmp/obj_tx_add_range_direct_0/testfile0
[line 19] Last 1 lines of /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/err0.log below (whole file has 1 lines):
[line 20] {obj_tx_add_range_direct.c:855 main} obj_tx_add_range_direct/TEST0: Error: pmemobj_create: No such file or directory
[line 21] Last 3 lines of /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/trace0.log below (whole file has 3 lines):
[line 22] {obj_tx_add_range_direct.c:846 main} obj_tx_add_range_direct/TEST0: START: obj_tx_add_range_direct
[line 23]  /home/jenkins-slave/workspace/PMDK-unittests-linux-py/pmdk_0/src/test/obj_tx_add_range_direct/obj_tx_add_range_direct /tmp/obj_tx_add_range_direct_0/testfile0
[line 24] {obj_tx_add_range_direct.c:855 main} obj_tx_add_range_direct/TEST0: Error: pmemobj_create: No such file or directory
[line 25] obj_tx_add_range_direct/TEST0: FAILED	(medium/nonpmem/debug/drd)
''')

    def test_output_from_windows_framework(self):
        example_input = r'''blk_non_zero/TEST8: SETUP (check\non-pmem\nondebug)
Access denied
    + CategoryInfo          : InvalidOperation: (:) [Get-WmiObject], ManagementException
    + FullyQualifiedErrorId : GetWMIManagementException,Microsoft.PowerShell.Commands.GetWmiObjectCommand
    + PSComputerName        : localhost

blk_non_zero/TEST8: SKIP not enough free space (1073741824b required)
blk_pool/TEST0: SETUP (check\non-pmem\debug)
blk_pool/TEST0: PASS			[00.306 s]
libpmempool_bttdev/TEST2: SETUP (check\pmem\nondebug)
libpmempool_bttdev/TEST2: PASS			[00.473 s]
libpmempool_bttdev/TEST3: SETUP (check\pmem\debug)
ftruncate: No space left on device
    + CategoryInfo          : NotSpecified: (ftruncate: No space left on devic
   e:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
    + PSComputerName        : localhost

libpmempool_bttdev/TEST3: failed with exit code -1073740791
    + CategoryInfo          : NotSpecified: (:) [Write-Error], WriteErrorExcep
   tion
    + FullyQualifiedErrorId : Microsoft.PowerShell.Commands.WriteErrorExceptio
   n,check_exit_code
    + PSComputerName        : localhost

C:\home\jenkins-slave\workspace\PMDK-unittests-windows\pmdk\src\test\libpmempool_bttdev\pmem3.log below.
<libpmem>: <1> [out.c:206 out_init] pid 7916: program: C:\home\jenkins-slave\workspace\PMDK-unittests-windows\pmdk\src\x64\Debug\tests\bttcreate.exe
<libpmem>: <1> [out.c:208 out_init] libpmem version 1.1
<libpmem>: <1> [out.c:212 out_init] src version: 1.8+git818.gcac9aa2b8
<libpmem>: <1> [out.c:240 out_init] compiled with support for shutdown state
<libpmem>: <3> [mmap.c:38 util_mmap_init]
<libpmem>: <3> [libpmem.c:27 libpmem_init]
<libpmem>: <3> [pmem.c:827 pmem_init]
<libpmem>: <3> [init.c:490 pmem2_arch_init]
<libpmem>: <3> [init.c:413 pmem_cpuinfo_to_funcs]
<libpmem>: <3> [init.c:416 pmem_cpuinfo_to_funcs] clflush supported
<libpmem>: <3> [init.c:424 pmem_cpuinfo_to_funcs] clflushopt supported
<libpmem>: <3> [init.c:437 pmem_cpuinfo_to_funcs] clwb supported
<libpmem>: <3> [init.c:468 pmem_cpuinfo_to_funcs] WC workaround = 1
<libpmem>: <3> [init.c:291 use_avx_memcpy_memset] avx supported
<libpmem>: <3> [init.c:299 use_avx_memcpy_memset] PMEM_AVX enabled
<libpmem>: <3> [init.c:403 use_avx512f_memcpy_memset] avx512f supported, but disabled at build time
<libpmem>: <3> [init.c:514 pmem2_arch_init] using clwb
<libpmem>: <3> [init.c:525 pmem2_arch_init] using movnt AVX
<libpmem>: <3> [auto_flush_windows.c:132 pmem2_auto_flush]
<libpmem>: <3> [auto_flush_windows.c:23 is_nfit_available] is_nfit_available()
<libpmem>: <3> [auto_flush_windows.c:105 parse_nfit_buffer] parse_nfit_buffer nfit_buffer NFITà, buffer_size 2016
<libpmem>: <3> [auto_flush_windows.c:84 is_auto_flush_cap_set] is_auto_flush_cap_set capa
NotSpecified: (:) [Write-Error], WriteErrorException
    + CategoryInfo          : NotSpecified: (:) [Write-Error], WriteErrorExcep
   tion
    + FullyQualifiedErrorId : Microsoft.PowerShell.Commands.WriteErrorExceptio
   n,fail
    + PSComputerName        : localhost

libpmempool_bttdev/TEST3: FAILEDlibpmempool_bttdev/TEST3: FAILED
    + CategoryInfo          : OperationStopped: (libpmempool_bttdev/TEST3: FAI
   LED:String) [], RuntimeException
    + FullyQualifiedErrorId : libpmempool_bttdev/TEST3: FAILED
    + PSComputerName        : localhost

C:\home\jenkins-slave\workspace\PMDK-unittests-windows\pmdk\src\test\RUNTESTS.P
S1 : RUNTESTS FAILED: one of the tests failed
At line:1 char:2
+  .\RUNTESTS.PS1 -i .\libpmempool_bttdev\ -o 24h
+  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (:) [Write-Error], WriteErrorExcep
   tion
    + FullyQualifiedErrorId : Microsoft.PowerShell.Commands.WriteErrorExceptio
   n,RUNTESTS.PS1
'''

        test_cases = pmdkMainProcessor.process(example_input)

        skipped = sum(isinstance(x.result, result.Skipped) for x in test_cases)
        errors = sum(isinstance(x.result, result.Error) for x in test_cases)
        failures = sum(isinstance(x.result, result.Failure) for x in test_cases)

        self.assertEqual(len(test_cases), 4)
        self.assertEqual(failures, 1)
        self.assertEqual(skipped, 1)
        self.assertEqual(errors, 0)

        self.assertEqual(test_cases[0].classname, "pmdkUnitTests.blk_non_zero")
        self.assertEqual(test_cases[1].classname, "pmdkUnitTests.blk_pool")
        self.assertEqual(test_cases[2].classname, "pmdkUnitTests.libpmempool_bttdev")
        self.assertEqual(test_cases[3].classname, "pmdkUnitTests.libpmempool_bttdev")

        self.assertEqual(test_cases[0].name, "TEST8")
        self.assertEqual(test_cases[1].name, "TEST0")
        self.assertEqual(test_cases[2].name, "TEST2")
        self.assertEqual(test_cases[3].name, "TEST3")

        self.assertIsInstance(test_cases[0].result, result.Skipped)
        self.assertIsInstance(test_cases[1].result, result.Passed)
        self.assertIsInstance(test_cases[2].result, result.Passed)
        self.assertIsInstance(test_cases[3].result, result.Failure)

        self.assertEqual(test_cases[0].result.message,
                         "SKIP not enough free space (1073741824b required)")

        self.assertEqual(test_cases[3].result.message, "libpmempool_bttdev/TEST3: FAILEDlibpmempool_bttdev/TEST3: FAILED")

        self.assertEqual(test_cases[0].stdout, r'''[line 2] Access denied
[line 3]     + CategoryInfo          : InvalidOperation: (:) [Get-WmiObject], ManagementException
[line 4]     + FullyQualifiedErrorId : GetWMIManagementException,Microsoft.PowerShell.Commands.GetWmiObjectCommand
[line 5]     + PSComputerName        : localhost
[line 6] 
[==line 7==] blk_non_zero/TEST8: SKIP not enough free space (1073741824b required)''')

        self.assertEqual(test_cases[1].stdout, r'''[line 4]     + FullyQualifiedErrorId : GetWMIManagementException,Microsoft.PowerShell.Commands.GetWmiObjectCommand
[line 5]     + PSComputerName        : localhost
[line 6] 
[line 7] blk_non_zero/TEST8: SKIP not enough free space (1073741824b required)
[line 8] blk_pool/TEST0: SETUP (check\non-pmem\debug)
[==line 9==] blk_pool/TEST0: PASS			[00.306 s]''')

        self.assertEqual(test_cases[2].stdout, r'''[line 6] 
[line 7] blk_non_zero/TEST8: SKIP not enough free space (1073741824b required)
[line 8] blk_pool/TEST0: SETUP (check\non-pmem\debug)
[line 9] blk_pool/TEST0: PASS			[00.306 s]
[line 10] libpmempool_bttdev/TEST2: SETUP (check\pmem\nondebug)
[==line 11==] libpmempool_bttdev/TEST2: PASS			[00.473 s]''')

        self.assertEqual(test_cases[3].stdout, r'''--- [approach: nearest matching SETUP] test trace for: libpmempool_bttdev/TEST3  ---
[line 12] libpmempool_bttdev/TEST3: SETUP (check\pmem\debug)
[line 13] ftruncate: No space left on device
[line 14]     + CategoryInfo          : NotSpecified: (ftruncate: No space left on devic
[line 15]    e:String) [], RemoteException
[line 16]     + FullyQualifiedErrorId : NativeCommandError
[line 17]     + PSComputerName        : localhost
[line 18] 
[line 19] libpmempool_bttdev/TEST3: failed with exit code -1073740791
[line 20]     + CategoryInfo          : NotSpecified: (:) [Write-Error], WriteErrorExcep
[line 21]    tion
[line 22]     + FullyQualifiedErrorId : Microsoft.PowerShell.Commands.WriteErrorExceptio
[line 23]    n,check_exit_code
[line 24]     + PSComputerName        : localhost
[line 25] 
[line 26] C:\home\jenkins-slave\workspace\PMDK-unittests-windows\pmdk\src\test\libpmempool_bttdev\pmem3.log below.
[line 27] <libpmem>: <1> [out.c:206 out_init] pid 7916: program: C:\home\jenkins-slave\workspace\PMDK-unittests-windows\pmdk\src\x64\Debug\tests\bttcreate.exe
[line 28] <libpmem>: <1> [out.c:208 out_init] libpmem version 1.1
[line 29] <libpmem>: <1> [out.c:212 out_init] src version: 1.8+git818.gcac9aa2b8
[line 30] <libpmem>: <1> [out.c:240 out_init] compiled with support for shutdown state
[line 31] <libpmem>: <3> [mmap.c:38 util_mmap_init]
[line 32] <libpmem>: <3> [libpmem.c:27 libpmem_init]
[line 33] <libpmem>: <3> [pmem.c:827 pmem_init]
[line 34] <libpmem>: <3> [init.c:490 pmem2_arch_init]
[line 35] <libpmem>: <3> [init.c:413 pmem_cpuinfo_to_funcs]
[line 36] <libpmem>: <3> [init.c:416 pmem_cpuinfo_to_funcs] clflush supported
[line 37] <libpmem>: <3> [init.c:424 pmem_cpuinfo_to_funcs] clflushopt supported
[line 38] <libpmem>: <3> [init.c:437 pmem_cpuinfo_to_funcs] clwb supported
[line 39] <libpmem>: <3> [init.c:468 pmem_cpuinfo_to_funcs] WC workaround = 1
[line 40] <libpmem>: <3> [init.c:291 use_avx_memcpy_memset] avx supported
[line 41] <libpmem>: <3> [init.c:299 use_avx_memcpy_memset] PMEM_AVX enabled
[line 42] <libpmem>: <3> [init.c:403 use_avx512f_memcpy_memset] avx512f supported, but disabled at build time
[line 43] <libpmem>: <3> [init.c:514 pmem2_arch_init] using clwb
[line 44] <libpmem>: <3> [init.c:525 pmem2_arch_init] using movnt AVX
[line 45] <libpmem>: <3> [auto_flush_windows.c:132 pmem2_auto_flush]
[line 46] <libpmem>: <3> [auto_flush_windows.c:23 is_nfit_available] is_nfit_available()
[line 47] <libpmem>: <3> [auto_flush_windows.c:105 parse_nfit_buffer] parse_nfit_buffer nfit_buffer NFITà, buffer_size 2016
[line 48] <libpmem>: <3> [auto_flush_windows.c:84 is_auto_flush_cap_set] is_auto_flush_cap_set capa
[line 49] NotSpecified: (:) [Write-Error], WriteErrorException
[line 50]     + CategoryInfo          : NotSpecified: (:) [Write-Error], WriteErrorExcep
[line 51]    tion
[line 52]     + FullyQualifiedErrorId : Microsoft.PowerShell.Commands.WriteErrorExceptio
[line 53]    n,fail
[line 54]     + PSComputerName        : localhost
[line 55] 
[==line 56==] libpmempool_bttdev/TEST3: FAILEDlibpmempool_bttdev/TEST3: FAILED
''')

    def test_ignoring_done_messages(self):
        """
        Test whether PaJaC will ignore prints like "obj_pool/TEST34: DONE" in the regular Linux framework.
        They should not be treated as separate tests.
        """
        example_input = r'''obj_pool/TEST34: SETUP (all/pmem/nondebug/pmemcheck)
[MATCHING FAILED, COMPLETE FILE (out34.log) BELOW]
obj_pool/TEST34: START: obj_pool
 ./obj_pool c /mnt/pmem1//test_obj_pool34ðŸ˜˜â â â ™â …É—PMDKÓœâ¥ºðŸ™‹/testfile test 32768 0600
/mnt/pmem1//test_obj_pool34ðŸ˜˜â â â ™â …É—PMDKÓœâ¥ºðŸ™‹/testfile: file size 34359738368 mode 0600
/mnt/pmem1//test_obj_pool34ðŸ˜˜â â â ™â …É—PMDKÓœâ¥ºðŸ™‹/testfile: pmemobj_check: Input/output error
obj_pool/TEST34: DONE

[EOF]
out34.log.match:1   obj_pool$(nW)TEST34: START: obj_pool$(nW)
out34.log:1         obj_pool/TEST34: START: obj_pool
out34.log.match:2    $(nW)obj_pool$(nW) c $(nW)testfile test$(nW) 32768 0600
out34.log:2          ./obj_pool c /mnt/pmem1//test_obj_pool34ðŸ˜˜â â â ™â …É—PMDKÓœâ¥ºðŸ™‹/testfile test 32768 0600
out34.log.match:3   $(nW)testfile: file size 34359738368 mode 0600
out34.log:3         /mnt/pmem1//test_obj_pool34ðŸ˜˜â â â ™â …É—PMDKÓœâ¥ºðŸ™‹/testfile: file size 34359738368 mode 0600
out34.log.match:4   obj_pool$(nW)TEST34: DONE
out34.log:4         /mnt/pmem1//test_obj_pool34ðŸ˜˜â â â ™â …É—PMDKÓœâ¥ºðŸ™‹/testfile: pmemobj_check: Input/output error
FAIL: match: out34.log.match:4 did not match pattern
RUNTESTS: stopping: obj_pool//TEST34 failed, TEST=all FS=any BUILD=nondebug
tput: No value for $TERM and no -T specified
tput: No value for $TERM and no -T specified
1 tests failed:
obj_pool//TEST34
********** obj_pool_lock/ **********'''
        test_cases = pmdkMainProcessor.process(example_input)

        skipped = sum(isinstance(x.result, result.Skipped) for x in test_cases)
        errors = sum(isinstance(x.result, result.Error) for x in test_cases)
        failures = sum(isinstance(x.result, result.Failure) for x in test_cases)

        self.assertEqual(len(test_cases), 1)
        self.assertEqual(failures, 1)
        self.assertEqual(skipped, 0)
        self.assertEqual(errors, 0)

        self.assertEqual(test_cases[0].classname, "pmdkUnitTests.obj_pool")
        self.assertEqual(test_cases[0].name, "TEST34")
        self.assertIsInstance(test_cases[0].result, result.Failure)

        self.assertEqual(test_cases[0].result.message,
                         "RUNTESTS: stopping: obj_pool//TEST34 failed, TEST=all FS=any BUILD=nondebug")

        self.assertEqual(test_cases[0].stdout, r'''--- [approach: nearest matching SETUP] test trace for: obj_pool/TEST34 TEST=all FS=any BUILD=nondebug ---
[line 1] obj_pool/TEST34: SETUP (all/pmem/nondebug/pmemcheck)
[line 2] [MATCHING FAILED, COMPLETE FILE (out34.log) BELOW]
[line 3] obj_pool/TEST34: START: obj_pool
[line 4]  ./obj_pool c /mnt/pmem1//test_obj_pool34ðŸ˜˜â â â ™â …É—PMDKÓœâ¥ºðŸ™‹/testfile test 32768 0600
[line 5] /mnt/pmem1//test_obj_pool34ðŸ˜˜â â â ™â …É—PMDKÓœâ¥ºðŸ™‹/testfile: file size 34359738368 mode 0600
[line 6] /mnt/pmem1//test_obj_pool34ðŸ˜˜â â â ™â …É—PMDKÓœâ¥ºðŸ™‹/testfile: pmemobj_check: Input/output error
[line 7] obj_pool/TEST34: DONE
[line 8] 
[line 9] [EOF]
[line 10] out34.log.match:1   obj_pool$(nW)TEST34: START: obj_pool$(nW)
[line 11] out34.log:1         obj_pool/TEST34: START: obj_pool
[line 12] out34.log.match:2    $(nW)obj_pool$(nW) c $(nW)testfile test$(nW) 32768 0600
[line 13] out34.log:2          ./obj_pool c /mnt/pmem1//test_obj_pool34ðŸ˜˜â â â ™â …É—PMDKÓœâ¥ºðŸ™‹/testfile test 32768 0600
[line 14] out34.log.match:3   $(nW)testfile: file size 34359738368 mode 0600
[line 15] out34.log:3         /mnt/pmem1//test_obj_pool34ðŸ˜˜â â â ™â …É—PMDKÓœâ¥ºðŸ™‹/testfile: file size 34359738368 mode 0600
[line 16] out34.log.match:4   obj_pool$(nW)TEST34: DONE
[line 17] out34.log:4         /mnt/pmem1//test_obj_pool34ðŸ˜˜â â â ™â …É—PMDKÓœâ¥ºðŸ™‹/testfile: pmemobj_check: Input/output error
[line 18] FAIL: match: out34.log.match:4 did not match pattern
[==line 19==] RUNTESTS: stopping: obj_pool//TEST34 failed, TEST=all FS=any BUILD=nondebug
''')
