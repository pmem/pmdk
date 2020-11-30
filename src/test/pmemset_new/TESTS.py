#!../envy
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
@g.no_testdir()
class PmemSetNewNoDir(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmemset_new', self.test_case)


class TEST0(PmemSetNewNoDir):
    """allocation and dealocation of pmemset_new"""
    test_case = "test_new_create_and_delete_valid"


class TEST1(PmemSetNewNoDir):
    """allocation of pmemset in case of no memory in system when set created"""
    test_case = "test_alloc_new_enomem"


class TEST2(PmemSetNewNoDir):
    """allocation of pmemset in case of no memory in system for ravl tree"""
    test_case = "test_alloc_new_tree_enomem"


class TEST3(PmemSetNewNoDir):
    """deleting null pmemset"""
    test_case = "test_delete_null_set"


class TEST4(PmemSetNewNoDir):
    """pmemset_new with invalid granularity value"""
    test_case = "test_granularity_not_set"
