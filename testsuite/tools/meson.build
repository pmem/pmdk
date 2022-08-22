ttools = {
    'gran_detecto': {},
    'pmemdetect': {},
    'usc_permission_check': {},
    'anonymous_mmap': {},
    'bttcreate': {},
    'cmpmap': {},
    'cpufd': {},
    'ctrld': {},
    'ddmap': {},
}

# This dependency is quite rough, but that's the way
# it was in src/tools/Makefile.inc
tools_common = static_library(
    'tools_common',
    objects:[pmempool.extract_objects('pmempool/common.c', 'pmempool/output.c'),
            libpmemblk.extract_objects('btt.c'),
        ],
)

tools_common_dep = declare_dependency(
    link_whole: tools_common,
    include_directories:[
        '../../src/tools/pmempool',
        '../../src/core',
        '../../src/common',
        '../../src/libpmemlog',
        '../../src/libpmemblk',
        '../../src/libpmemobj'
        ],
)

foreach ttool, args : ttools
    subdir(ttool)
endforeach