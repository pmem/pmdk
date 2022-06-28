sources = files(
    'rpmem_addr_ext.c',
)

dependencies = [
    librpmem_dep,
    unittest_dep,
]

c_args = []
include_directories = [
    include_directories('../../rpmem_common'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
