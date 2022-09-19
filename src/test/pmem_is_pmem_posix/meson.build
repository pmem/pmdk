sources = files(
    'pmem_is_pmem_posix.c',
)

dependencies = [
    libpmem_internal_dep,
    pmem2arch_dep,
    unittest_dep,
    pmemcommon_dep,
]

# Those arguments were present in the Makefile buildsystem,
# but they cause redefinition warnings
c_args = [
    # '-DSRCVERSION=""',
    # '-DDEBUG',
    # '-DMOVDIR64B_AVAILABLE=0',
    # '-DAVX512F_AVAILABLE=0',
    # '-DAVX_AVAILABLE=0',
    # '-DSSE2_AVAILABLE=0',
]
include_directories = [
    include_directories('../../libpmem'),
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
