#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2023, Intel Corporation
#

import testframework as t
from testframework import granularity as g
import valgrind as vg


# These tests last too long under drd
# Exceptions: test no. 2
@t.require_valgrind_disabled('drd')
class ObjDefragAdvanced(t.BaseTest):
    test_type = t.Short

    max_nodes = 50
    max_edges = 10
    graph_copies = 10
    pool_size = 500 * t.MiB
    max_rounds = 10
    min_root_size = 0

    def run(self, ctx):
        ctx.require_free_space(self.pool_size)

        path = ctx.create_holey_file(self.pool_size, 'testfile')
        dump1 = 'dump_1_{}.log'.format(self.testnum)
        dump2 = 'dump_2_{}.log'.format(self.testnum)

        ctx.exec('obj_defrag_advanced',
                 'op_pool_create', path,
                 'op_graph_create', str(self.max_nodes), str(self.max_edges),
                 str(self.graph_copies), str(self.min_root_size),
                 'op_graph_dump', dump1,
                 'op_graph_defrag', str(self.max_rounds),
                 'op_graph_dump', dump2,
                 'op_pool_close',
                 'op_dump_compare', dump1, dump2)


class TEST0(ObjDefragAdvanced):
    max_nodes = 5
    max_edges = 5
    graph_copies = 5


class TEST1(ObjDefragAdvanced):
    max_nodes = 2048
    max_edges = 5
    graph_copies = 5


@t.require_valgrind_disabled('helgrind')
@g.require_granularity(g.CACHELINE)
class TEST2(ObjDefragAdvanced):
    test_type = t.Medium
    # XXX port this to the new framework
    # Restore defaults
    drd = vg.AUTO

    max_nodes = 512
    max_edges = 64
    graph_copies = 5
    min_root_size = 4096


@g.require_granularity(g.CACHELINE)
class ObjDefragAdvancedMt(ObjDefragAdvanced):
    test_type = t.Medium

    nthreads = 2
    ncycles = 2

    def run(self, ctx):
        ctx.require_free_space(self.pool_size)

        path = ctx.create_holey_file(self.pool_size, 'testfile')

        ctx.exec('obj_defrag_advanced',
                 'op_pool_create', path,
                 'op_graph_create_n_defrag_mt', self.max_nodes,
                 self.max_edges, self.graph_copies, self.min_root_size,
                 self.max_rounds, self.nthreads, self.ncycles, self.testnum,
                 'op_pool_close')


class TEST3(ObjDefragAdvancedMt):
    test_type = t.Long

    max_nodes = 256
    max_edges = 64
    graph_copies = 10
    nthreads = 1
    ncycles = 25


class TEST4(ObjDefragAdvancedMt):
    max_nodes = 128
    max_edges = 32
    graph_copies = 10
    nthreads = 10
    ncycles = 25


# XXX disable the test for 'drd'
# until https://github.com/pmem/pmdk/issues/5690 is fixed.
# previousely the test has been disabled for other Valgrind options
# This test last too long under helgrind/memcheck/pmemcheck
# @t.require_valgrind_disabled('helgrind', 'memcheck', 'pmemcheck')
@t.require_valgrind_disabled('helgrind', 'memcheck', 'pmemcheck', 'drd')
class TEST5(ObjDefragAdvancedMt):

    max_nodes = 256
    max_edges = 32
    graph_copies = 5
    nthreads = 10
    ncycles = 25


# a testcase designed to verify the pool content in case of fail
# class TESTX(ObjDefragAdvanced):
#     def run(self, ctx):
#         path = '/custom/pool/path'
#         dump_prefix = 'dump'
#
#         ctx.exec('obj_defrag_advanced',
#             'op_pool_open', path,
#             'op_graph_dump_all', dump_prefix,
#             'op_pool_close')
