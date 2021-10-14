#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2021, Intel Corporation
#

import testframework as t
from testframework import granularity as g
import os


@g.require_granularity(g.ANY)
class PMEMSET_SOURCE(t.Test):
    test_type = t.Short
    create_file = True

    def run(self, ctx):
        if self.create_file:
            filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        else:
            filepath = os.path.join(ctx.testdir, 'testfile1')

        ctx.exec('pmemset_source', self.test_case, filepath)
        if os.path.exists(filepath):
            os.remove(filepath)


@t.windows_exclude
@t.require_devdax(t.DevDax('devdax1'))
class PMEMSET_SOURCE_DEVDAX(t.Test):
    test_type = t.Short
    create_file = True

    def run(self, ctx):
        ddpath = ctx.devdaxes.devdax1.path
        ctx.exec('pmemset_source', self.test_case, ddpath)


@g.no_testdir()
class PMEMSET_SOURCE_NO_DIR(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmemset_source', self.test_case)


class PMEMSET_SOURCE_DIR_ONLY(t.Test):
    test_type = t.Short
    do_not_close = False

    def run(self, ctx):
        if self.do_not_close:
            ctx.env['UNITTEST_DO_NOT_FAIL_OPEN_FILES'] = '1'
        ctx.exec('pmemset_source', self.test_case, ctx.testdir)


class TEST0(PMEMSET_SOURCE):
    """allocation of pmemset_source in case of missing memory in system"""
    test_case = "test_alloc_src_enomem"


class TEST1(PMEMSET_SOURCE_NO_DIR):
    """testing pmemset_from_pmem2 with null value"""
    test_case = "test_set_from_pmem2_null"


class TEST2(PMEMSET_SOURCE):
    """valid allocation of pmemset_source from pmem2"""
    test_case = "test_set_from_pmem2_valid"


class TEST3(PMEMSET_SOURCE_NO_DIR):
    """test source creation from null file path"""
    test_case = "test_src_from_file_null"


class TEST4(PMEMSET_SOURCE):
    """test source creation with valid file path"""
    test_case = "test_src_from_file_valid"


class TEST5(PMEMSET_SOURCE):
    """test source creation with existing file and create_always flag set"""
    test_case = "test_src_from_file_exists_always_disp"


class TEST6(PMEMSET_SOURCE):
    """test source creation with no existing file and create_always flag set"""
    test_case = "test_src_from_file_not_exists_always_disp"
    create_file = False


class TEST7(PMEMSET_SOURCE):
    """test source creation with existing file and if_needed flag set"""
    test_case = "test_src_from_file_exists_needed_disp"


class TEST8(PMEMSET_SOURCE):
    """test source creation with no existing file and if_needed flag set"""
    test_case = "test_src_from_file_not_exists_needed_disp"
    create_file = False


class TEST9(PMEMSET_SOURCE):
    """test source creation with invalid flags"""
    test_case = "test_src_from_file_invalid_flags"


class TEST10(PMEMSET_SOURCE_DIR_ONLY):
    """testing pmemset_from_temporary valid case"""
    test_case = "test_src_from_temporary_valid"


class TEST11(PMEMSET_SOURCE_NO_DIR):
    """testing pmemset_from_temporary invalid dir"""
    test_case = "test_src_from_temporary_inval_dir"


class TEST12(PMEMSET_SOURCE_DIR_ONLY):
    """testing pmemset_from_temporary and skip pmemset source delete"""
    test_case = "test_src_from_temporary_no_del"
    do_not_close = True


class TEST13(PMEMSET_SOURCE):
    """test source creation with no existing file and do_not_grow flag set"""
    test_case = "test_src_from_file_with_do_not_grow"
    create_file = False


@t.windows_exclude
class TEST14(PMEMSET_SOURCE):
    """test source creation with rusr file mode"""
    test_case = "test_src_from_file_with_rusr_mode"
    create_file = False


@t.windows_exclude
class TEST15(PMEMSET_SOURCE):
    """test source creation with rwxu file mode"""
    test_case = "test_src_from_file_with_rwxu_mode"
    create_file = False


@t.windows_exclude
class TEST16(PMEMSET_SOURCE):
    """test source creation with numeric file mode"""
    test_case = "test_src_from_file_with_num_mode"
    create_file = False


@t.windows_exclude
class TEST17(PMEMSET_SOURCE):
    """test source creation with inval file mode"""
    test_case = "test_src_from_file_with_inval_mode"
    create_file = False


@t.windows_exclude
class TEST18(PMEMSET_SOURCE):
    """test source creation with only file mode"""
    test_case = "test_src_from_file_only_mode"
    create_file = False


@t.windows_only
class TEST19(PMEMSET_SOURCE):
    """test source creation with inval file mode on Windows"""
    test_case = "test_src_from_file_with_inval_win_mode"
    create_file = False


@t.windows_exclude
class TEST20(PMEMSET_SOURCE):
    """test source creation with rusr file mode if needed"""
    test_case = "test_src_from_file_with_rusr_mode_if_needed"
    create_file = False


@t.windows_exclude
class TEST21(PMEMSET_SOURCE):
    """test source creation with rwxu file mode if needed, created file"""
    test_case = "test_src_from_file_with_rwxu_mode_if_needed_created"
    create_file = True


class TEST22(PMEMSET_SOURCE):
    """test mcsafe read operation"""
    test_case = "test_src_mcsafe_read"
    create_file = True


class TEST23(PMEMSET_SOURCE):
    """test mcsafe write operation"""
    test_case = "test_src_mcsafe_write"
    create_file = True


class TEST24(PMEMSET_SOURCE_DEVDAX):
    """test mcsafe read operation on devdax"""
    test_case = "test_src_mcsafe_read"


class TEST25(PMEMSET_SOURCE_DEVDAX):
    """test mcsafe write operation on devdax"""
    test_case = "test_src_mcsafe_write"


class TEST26(PMEMSET_SOURCE):
    """test alignment read operation"""
    test_case = "test_src_alignment"
    create_file = True


class TEST27(PMEMSET_SOURCE_DEVDAX):
    """test alignment read operation"""
    test_case = "test_src_alignment"
    create_file = True
