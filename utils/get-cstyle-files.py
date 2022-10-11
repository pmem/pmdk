#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation
#

"""
Helper script for cstyle, which recursively obtains
all of the files with .h or .c extensions, starting in
the root directory passed as a parameter.
Excluded files are containted in the .cstyleignore file.
"""

import os
import sys
import re

if len(sys.argv) < 3:
    print("Wrong number of arguments!"
        "Usage: get-cstyle-files.py root_dir build_dir")
    sys.exit(1)

root_directory = sys.argv[1]
# the path to build directory must be changed into a relative one
# because paths to excluded files must be relative by convention
build_directory = os.path.relpath(sys.argv[2])

cstyle_ignore_f = os.path.join(root_directory, "utils", ".cstyleignore")

with open(cstyle_ignore_f) as file:
    exclude_file_paths = file.readlines()
    exclude_file_paths = [exclude_file_path.rstrip() for 
        exclude_file_path in exclude_file_paths]

# since build directory may differ in name, it must be obtained
# as parameter, instead of from the .cstyleignore file
exclude_file_paths.append(os.path.join(build_directory, ".+"))

excluded = 0
for path, dirs, files in os.walk(root_directory):
    for file in files:
        file_path = os.path.join(path, file)
        for exclude_file_path in exclude_file_paths:
            # force the string to be the exact match to avoid
            # excluding files unintentionally
            exclude_file_path = ''.join(('^', exclude_file_path,'$'))
            if re.search(exclude_file_path, os.path.relpath(file_path)):
                excluded = 1
                break
        if not excluded:
            match = re.search("\.(h|c)$", file)
            if match:
                print(file_path)
        excluded = 0
