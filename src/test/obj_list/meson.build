# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'obj_list_mocks_palloc.c',
    'obj_list_mocks.c',
    'obj_list.c',
)

dependencies = [
    libpmem_dep,
    libpmemobj_internal_dep,
    unittest_dep,
]

c_args = []
link_args = []
include_directories = []

# scan test source files for mock functions
ret = run_command(
        get_mocks,
        sources,
        check: true,
)

mock_funcs = ret.stdout().strip().split('\n')
if mock_funcs.length() != 0
    wrap_arg_str = '-Wl'
    foreach mock : mock_funcs
        wrap_arg_str += f',--wrap=@mock@'
    endforeach
    link_args += [wrap_arg_str]
endif

executable(
    t,
    sources,
    c_args: c_args,
    link_args: link_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
