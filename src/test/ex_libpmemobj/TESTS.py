#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#
import os
import testframework as t
import futils

TEST_PARAMS = ['hashmap_tx', 'hashmap_atomic', 'hashmap_rp',
               'ctree', 'btree', 'rtree',
               'rbtree', 'skiplist']


@t.require_build('debug')
@t.require_command('gdb')
@t.windows_exclude
class EX_LIBPMEMOBJ(t.Test):
    test_type = t.Medium
    input = ['i 3', 'q']

    def _run_gdb(self, example_path,
                 program_args, b_fun_name):
        gdb_process = self.ctx.gdb(self.testnum, example_path,
                                   program_args, '--batch')
        gdb_process.add_command('set breakpoint pending on')
        gdb_process.add_command(' '.join(['b', b_fun_name]))
        gdb_process.add_command('run')
        gdb_process.add_command('quit')
        gdb_process.execute()


# Test for exit before POBJ_ZNEW
@t.add_params('parameters', TEST_PARAMS)
class TEST26(EX_LIBPMEMOBJ):
    def run(self, ctx):
        example_path = futils.get_example_path(ctx, 'pmemobj',
                                               'mapcli', dirname='map')
        testfile = os.path.join(ctx.testdir, 'testfile1')
        program_args = ' '.join([ctx.parameters(), testfile])
        fun_name = 'map_create'

        self._run_gdb(example_path, program_args, fun_name)
        ctx.exec(example_path, ctx.parameters(),
                 testfile, std_input=self.input)


# Test for exit after POBJ_ZNEW
@t.add_params('parameters', TEST_PARAMS)
class TEST27(EX_LIBPMEMOBJ):
    def run(self, ctx):
        example_path = futils.get_example_path(ctx, 'pmemobj',
                                               'mapcli', dirname='map')
        testfile = os.path.join(ctx.testdir, 'testfile1')
        arg_fun_dict = {'hashmap_tx': 'create_hashmap',
                        'hashmap_atomic': "create_hashmap",
                        'hashmap_rp': "hashmap_create",
                        'ctree': 'ctree_map_create',
                        'btree': 'btree_map_create',
                        'rtree': 'rtree_map_create',
                        'rbtree': 'rbtree_map_create',
                        'skiplist': 'skiplist_map_create'}
        program_args = ' '.join([ctx.parameters(), testfile])
        fun_name = arg_fun_dict.get(ctx.parameters())

        self._run_gdb(example_path, program_args, fun_name)
        ctx.exec(example_path, ctx.parameters(),
                 testfile, std_input=self.input)
