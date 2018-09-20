import sys
sys.path.insert(1, '../unittest')
from unittest import *

# each class is treated as a test
# example tests:


class launch0:
    def run(self, ctx):
        filepath = ctx.create_holey_file(MB(16), "testfile1")
        ctx.test_exec(F'./obj_basic_integration {filepath}')


executor().run(launch0(), build(debug, static_debug), fs(pmem))


class launch1:
    def run(self, ctx):
        filepath = ctx.create_holey_file(MB(16), "testfile1")
        ctx.test_exec(F'./obj_basic_integration {filepath}')


executor().run(launch1(), build(debug, nondebug))
