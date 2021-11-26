#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

import testframework as t
from testframework import granularity as g
import futils
import re
import os

@g.require_granularity(g.ANY)
class PMEM_EADR(t.Test):
    test_type = t.Short
    envs0 = ()
    envs1 = ()

    functionname = None
    isa = "((sse2)|(avx)|(avx512f))"
    perfbarrier = "((_wcbarrier)|(_nobarrier)|())"

    def run(self, ctx):
        for env in self.envs0:
            ctx.env[env] = '0'
        for env in self.envs1:
            ctx.env[env] = '1'

        log_file = os.path.join(self.cwd, F'pmem_{self.testnum}.log')
        file_path = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.env['PMEM_LOG_LEVEL'] = '15'
        ctx.exec('pmem_eADR_functions', self.test_case, file_path,)

        log_content = open(log_file).read()

        regex1 = F"{self.function_name}_mov_{self.isa}_noflush"
        regex2 = F"{self.function_name}_movnt_{self.isa}_empty{self.perfbarrier}"
        regex3 = F"{self.function_name}_mov_{self.isa}_empty"
        regex = F"({regex1})|({regex2})|({regex3})"

        match = re.search(regex, log_content)

        if match == None:
            futils.fail(F"Failed to find any occurence of memove with eADR in pmem_{self.testnum}.log")


@t.require_build('debug')
class TEST0(PMEM_EADR):
    function_name = "memmove"
    envs1 = ('PMEM_NO_FLUSH',)
    test_case = 'test_eADR_memmove_256B'

@t.require_build('debug')
class TEST1(PMEM_EADR):
    function_name = "memmove"
    envs1 = ('PMEM_NO_FLUSH',)
    test_case = 'test_eADR_memmove_16MiB'

@t.require_build('debug')
class TEST2(PMEM_EADR):
    function_name = "memset"
    envs1 = ('PMEM_NO_FLUSH',)
    test_case = 'test_eADR_memset_256B'

@t.require_build('debug')
class TEST3(PMEM_EADR):
    function_name = "memset"
    envs1 = ('PMEM_NO_FLUSH',)
    test_case = 'test_eADR_memset_16MiB'
