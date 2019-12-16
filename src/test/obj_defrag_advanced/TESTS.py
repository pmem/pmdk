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

import difflib
import os

import testframework as t
import futils


class OBJ_DEFRAG_ADVANCED(t.BaseTest):
    test_type = t.Medium
    max_nodes = 50
    max_edges = 10
    max_graph_copies = 10

    def match_dumps(self, path1, path2):
        with open(path1, 'r') as file1, open(path2, 'r') as file2:
            diff = list(difflib.ndiff(file1.readlines(), file2.readlines()))
            delta = [x for x in diff if x.startswith('- ') or x.startswith('+ ')]
            if len(delta) > 0:
                futils.fail(''.join(diff))

    def run(self, ctx):
        filepath = ctx.create_holey_file(500 * t.MiB, 'testfile')
        dump1 = os.path.join(ctx.testdir, 'dump1')
        dump2 = os.path.join(ctx.testdir, 'dump2')
        ctx.exec('obj_defrag_advanced', '--create', '--path=' + filepath,
            '--max-nodes=' + str(self.max_nodes),
            '--max-edges=' + str(self.max_edges),
            '--max-graph-copies=' + str(self.max_graph_copies))
        ctx.exec('obj_defrag_advanced', '--dump=' + dump1, '--path=' + filepath)
        ctx.exec('obj_defrag_advanced', '--defrag', '--path=' + filepath)
        ctx.exec('obj_defrag_advanced', '--dump=' + dump2, '--path=' + filepath)
        self.match_dumps(dump1, dump2)

# XXX these test cases are only examples. When obj defragmentation will be fixed.
# Here should come misc complex cases for testing this functionality.

# pass
class TEST0(OBJ_DEFRAG_ADVANCED):
    max_nodes = 5
    max_edges = 5
    max_graph_copies = 1

# fail
class TEST1(OBJ_DEFRAG_ADVANCED):
    max_nodes = 23
    max_edges = 1
    max_graph_copies = 1
