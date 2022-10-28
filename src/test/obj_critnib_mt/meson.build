# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'obj_critnib_mt.c',
)

dependencies = [
    libpmemobj_dep,
    unittest_dep,
    pmemcommon_dep,
]

c_args = []
include_directories = [
    include_directories('../../libpmemobj'),
]

executable(
    t,
    sources,
    objects: [
	libpmemobj.extract_objects('critnib.c'),
    ],
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
