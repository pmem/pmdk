sources = files(
    'log_pool_win.c',
)

dependencies = [
    libpmemlog_dep,
    unittest_dep,
]

c_args = []
include_directories = []

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)

# TODO: 'pmemspoil' dependency
