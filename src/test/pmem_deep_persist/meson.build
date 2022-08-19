sources = files(
    'pmem_deep_persist.c',
    '../../libpmem/pmem.c',
    '../../libpmem2/memops_generic.c',
)

if host_machine.system() != 'windows'
    sources += files('../../libpmem/pmem_posix.c')
    sources += files('mocks_posix.c')
endif

dependencies = [
    libpmemobj_internal_dep,
    pmem2arch_dep,
    unittest_dep,
]

c_args = []
link_args = []
include_directories = [
    include_directories('../../libpmem'),
    include_directories('../../libpmem2'),
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
