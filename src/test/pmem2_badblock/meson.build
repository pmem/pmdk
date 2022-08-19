sources = files(
    'pmem2_badblock.c',
)

dependencies = [
    libpmem2_internal_dep,
    unittest_pmem2_dep,
    unittest_dep,
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

copy_target = custom_target(t+'(test-files)',
    input : test_files,
    output :  test_files,
    command : [copy, '-i', '@INPUT@', '-o', '@OUTPUT@'],
    install : false,
    build_by_default : true
)
