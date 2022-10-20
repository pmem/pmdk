# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'obj_sync.c',
    '../../libpmemobj/sync.c',
)

if host_machine.system() == 'windows'
    sources += files('mocks_windows.c')
else
    sources += files('mocks_posix.c')
endif

dependencies = [
    pmemcommon_dep,
    unittest_dep,
]

c_args = []
link_args = []
include_directories = [
    include_directories('../../libpmemobj'),
]

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
