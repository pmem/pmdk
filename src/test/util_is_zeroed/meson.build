sources = files(
    'util_is_zeroed.c',
)

dependencies = [
    unittest_dep,
    pmemcommon_dep,
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
