sources = files(
    'rpmem_obc_test_close.c',
    'rpmem_obc_test_common.c',
    'rpmem_obc_test_create.c',
    'rpmem_obc_test_misc.c',
    'rpmem_obc_test_open.c',
    'rpmem_obc_test_set_attr.c',
    'rpmem_obc_test.c',
)

dependencies = [
    librpmem_dep,
    rpmemd_dep,
]

c_args = []
include_directories = []

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies + test_dependencies,
)
