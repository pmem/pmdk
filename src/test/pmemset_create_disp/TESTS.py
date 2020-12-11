#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

import testframework as t
from testframework import granularity as g
import os


@g.require_granularity(g.ANY)
class PmemsetCreateDisp(t.Test):
    test_type = t.Short
    create_file = False

    def run(self, ctx):
        if self.create_file:
            filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        else:
            filepath = os.path.join(ctx.testdir, 'testfile1')
        ctx.exec('pmemset_create_disp', self.test_case, filepath)
        if os.path.exists(filepath):
            os.remove(filepath)


class TEST0(PmemsetCreateDisp):
    """test valid pmemset_config_file_create_disposition values"""
    test_case = "test_config_file_create_dispostion_valid"


class TEST1(PmemsetCreateDisp):
    """test invalid pmemset_config_file_create_disposition values"""
    test_case = "test_config_file_create_dispostion_invalid"


class TEST2(PmemsetCreateDisp):
    """test pmemset_config_file_create_disposition values when file exists"""
    test_case = "test_file_create_disp_file_exists"
    create_file = True


class TEST3(PmemsetCreateDisp):
    """test create_always disposition when file does not exist"""
    test_case = "test_file_create_disp_no_file_always"
    create_file = False


class TEST4(PmemsetCreateDisp):
    """test create_if_needed disposition when file does not exist"""
    test_case = "test_file_create_disp_no_file_needed"
    create_file = False


class TEST5(PmemsetCreateDisp):
    """test open only disposition when file does not exist"""
    test_case = "test_file_create_disp_no_file_open"
    create_file = False
