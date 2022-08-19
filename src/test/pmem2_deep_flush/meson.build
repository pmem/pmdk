sources = files(
    'pmem2_deep_flush.c',
    '../../libpmem2/deep_flush.c',
    '../../libpmem2/memops_generic.c',
    '../../libpmem2/persist.c',
    '../../libpmem2/errormsg.c',
)

if host_machine.system() == 'linux'
    sources += files(
        '../../libpmem2/deep_flush_linux.c',
    )
endif

dependencies = [
    pmemcore_dep,
    unittest_pmem2_dep,
    unittest_dep,
]

c_args = []
link_args = []
include_directories = [
    include_directories('../../libpmem2'),
    miniasync_include,
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
