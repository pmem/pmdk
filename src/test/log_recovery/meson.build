sources = files(
    'log_recovery.c',
)

dependencies = [
    libpmemlog_dep,
    unittest_dep,
]

c_args = []
include_directories = [
    include_directories('../../libpmemlog'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
