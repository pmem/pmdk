#!/usr/bin/env python3
#
# Copyright 2019, Intel Corporation
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

"""
Check whether 'make pcheck' indeed runs all available tests,
detected as directories in src/test tree with at least one 'TEST[number]' file.
Test list run by 'make pcheck' is provided to the script standard input.
The script is meant to be run internally by 'make pcheck-check' target
rather than directly.
"""

import argparse
import os
import sys
from os import path


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tests_rootdir', help='Tests root directory')
    args = parser.parse_args()
    return args


def has_test_file(dir_):
    for ent in os.listdir(dir_):
        if path.isfile(path.join(dir_, ent)) and ent.startswith('TEST')\
           and not ent.endswith('.PS1'):
            return True
    return False


def get_tree_testdirs(tests_rootdir):
    dirs = []
    for ent in os.listdir(tests_rootdir):
        if path.isdir(path.join(tests_rootdir, ent)):
            dirs.append(ent)

    testdirs = [d for d in dirs if has_test_file(path.join(tests_rootdir, d))]
    return testdirs


def main():
    args = parse_args()

    make_testdirs = [d.strip() for d in sys.stdin.read().split()]
    tree_testdirs = get_tree_testdirs(args.tests_rootdir)
    not_run = []
    for tt in tree_testdirs:
        if tt not in make_testdirs:
            not_run.append(tt)

    if not_run:
        print('These tests are not run by "make pcheck":',
              *not_run, sep=os.linesep)
        sys.exit(1)


if __name__ == '__main__':
    main()
