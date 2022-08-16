sources = files(
    'pmemset_part.c',
)

dependencies = [
    libpmem2_dep,
    libpmemset_internal_dep,
    unittest_pmemset_dep,
    unittest_dep,
]

c_args = []
include_directories = [
    include_directories('../../libpmemset'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)

custom_target(t+'(test-files)',
    input : test_files,
    output :  test_files,
    command : [copy, '-i', '@INPUT@', '-o', '@OUTPUT@'],
    install : false,
    build_by_default : true
)
