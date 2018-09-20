import sys
sys.path.insert(0, '../unittest')
from unittest import *

class launch0:
    def run(self, ctx):
        filepath = ctx.create_holey_file(MB(16), "testfile1")
        ctx.test_exec(F'./obj_bucket {filepath}')

executor().run(launch0(), build(debug, nondebug), fs()) #trace_disable(memcheck)

class launch3:
    def run(self, ctx):
        filepath = ctx.create_holey_file(MB(16), "testfile1")
        ctx.test_exec(F'./obj_bucket {filepath}')

executor().run(launch3()) 