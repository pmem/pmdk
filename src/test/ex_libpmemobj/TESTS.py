#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#
import os
import sys
import testframework as t
import futils
import python_gdb


@t.require_build(["debug"])
@t.require_command("gdb")
class EX_LIBPMEMOBJ(t.Test):
    test_type = t.Medium


class TEST26(EX_LIBPMEMOBJ):

    def run(self, ctx):
        path_to_test_dir = ctx.cwd
        # XXX get_example_path from futils doesn't return a correct path.
        # It is attempted to fix manually below.
        example_path = futils.get_example_path(ctx, "pmemobj", "mapcli")
        if sys.platform != 'win32':
            example_path = example_path.replace("/mapcli/mapcli",
                                                "/map/mapcli")
        testfile = os.path.join(ctx.testdir, "testfile1")
        arg_fun_dict = {"hashmap_tx": "create_hashmap",
                        "hashmap_atomic": "create_hashmap",
                        "hashmap_rp": "hashmap_create",
                        "ctree": "ctree_map_create",
                        "btree": "btree_map_create",
                        "rtree": "rtree_map_create",
                        "rbtree": "rbtree_map_create",
                        "skiplist": "skiplist_map_create"}
        b_fun_name = "map_create"
        input = ["i 3", "q"]

        # Test for exit before POBJ_ZNEW
        for key in arg_fun_dict.keys():
            self._run_gdb(path_to_test_dir, key, b_fun_name,
                          example_path, testfile)
            ctx.exec(example_path, key, testfile, std_input=input)
            os.remove(testfile)

        # Test for exit after POBJ_ZNEW
        for key, value in arg_fun_dict.items():
            self._run_gdb(path_to_test_dir, key, value, example_path, testfile)
            ctx.exec(example_path, key, testfile, std_input=input)
            os.remove(testfile)

    def _run_gdb(self, path_to_test_dir, program_arg,
                 b_fun_name, example_path, testfile):
        program_args = ''.join([program_arg, " ", testfile])
        gdb_process = python_gdb.GdbProcess(path_to_test_dir, example_path,
                                            program_args, "--batch")
        gdb_process.command("set breakpoint pending on")
        gdb_process.command(''.join(["b ", b_fun_name]))
        gdb_process.command("run")
        gdb_process.command("quit")
        gdb_process.execute()
