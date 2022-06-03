sources = files(
    'rpmemd_db_test.c',
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
