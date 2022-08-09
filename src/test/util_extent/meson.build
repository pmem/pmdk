sources = files(
    'util_extent.c',
)

dependencies = [
    libpmem2_dep,
    unittest_dep,
    pmemcommon_dep,
]

c_args = []
include_directories = [
    include_directories('../../libpmem2'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
