sources = files(
    'out_err_mt.c',
)

dependencies = [
    libpmem_dep,
    libpmemblk_dep,
    libpmemlog_dep,
    libpmemobj_dep,
    libpmempool_dep,
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
