#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation
#

"""
Script parsing the 'FUNC_MOCK_RET_ALWAYS_VOID', 'FUNC_MOCK_RET_ALWAYS' and
'FUNC_MOCK' macros from input source code files.
It prints a list of functions mocked with these macros.
"""

import sys

if len(sys.argv) < 2:
    print("Wrong number of arguments! Usage: get-mocks.py file1, file2, ...")
    sys.exit(1)

c_files = sys.argv[1:]

# macros used to mark mocked C functions
mock_macros = ['FUNC_MOCK_RET_ALWAYS_VOID', 'FUNC_MOCK_RET_ALWAYS',
               'FUNC_MOCK']

for file in c_files:
    with open(file, 'r') as file:
        filedata = file.read()

    # get the mocked function names
    for line in filedata.split('\n'):
        for macro in mock_macros:
            if ((macro + "(") in line):
                mock_func = line.split(macro + "(")[1].split(',')[0]
                print(mock_func)
