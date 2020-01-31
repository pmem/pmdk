#!../env.py
#
# Copyright 2020, Intel Corporation
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
        path = ctx.create_holey_file(self.pool_size, 'testfile')
        dump1 = 'dump1.log'
        dump2 = 'dump2.log'

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
        path = ctx.create_holey_file(self.pool_size, 'testfile')

        ctx.exec('obj_defrag_advanced',
                 'op_pool_create', path,
                 'op_graph_create_n_defrag_mt', str(self.max_nodes),
                 str(self.max_edges), str(self.graph_copies),
                 str(self.min_root_size), str(self.max_rounds),
                 str(self.nthreads), str(self.ncycles),
                 'op_pool_close')


class TEST3(ObjDefragAdvancedMt):
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


# This test last too long under helgrind/memcheck/pmemcheck
@t.require_valgrind_disabled(['helgrind', 'memcheck', 'pmemcheck'])
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
