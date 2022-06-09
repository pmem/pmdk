sources = files(
    'rpmemd_obc_test_close.c',
    'rpmemd_obc_test_common.c',
    'rpmemd_obc_test_create.c',
    'rpmemd_obc_test_misc.c',
    'rpmemd_obc_test_msg_hdr.c',
    'rpmemd_obc_test_open.c',
    'rpmemd_obc_test_set_attr.c',
    'rpmemd_obc_test.c',
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
