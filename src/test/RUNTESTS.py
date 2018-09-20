#!/usr/bin/env python3
import sys
import os
from pathlib import Path
from py_parser import modules

curdir = os.getcwd() + "/"


#
# run_tests --- search for all TESTS.py files and run all of them
#
def run_tests():
    for i in range(len(modules)):
        argv = ' '.join(sys.argv[1:])
        # set current working directory to the directory with TESTS.py
        os.chdir(Path(curdir + modules[i].replace('TESTS.py', '')))
        os.system(F"python3 TESTS.py {argv}")


run_tests()
