#!/usr/bin/env python3

import os, sys, shutil

copy_prefixes = sys.argv[3]
copy_suffixes = sys.argv[4]

# get absolute input and output paths
input_path = os.path.join(
    os.getenv('MESON_SOURCE_ROOT'),
    os.getenv('MESON_SUBDIR'),
    sys.argv[1])

output_path = os.path.join(
    os.getenv('MESON_BUILD_ROOT'),
    os.getenv('MESON_SUBDIR'),
    sys.argv[2])

# make sure destination directory exists
os.makedirs(output_path, exist_ok=True)

# copy files with matching prefixes or suffixes
for file in os.listdir(input_path):
    if file.startswith(copy_prefixes) or file.endswith(copy_suffixes):
        shutil.copy(os.path.join(input_path, file), os.path.join(output_path, file))
