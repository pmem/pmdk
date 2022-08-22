ttools = {
    'anonymous_mmap': {},
    'bttcreate': {},
    'cmpmap': {},
    'cpufd': {},
    'ctrld': {},
    'ddmap': {},
    'extents': {},
    'fallocate_detect': {},
    'gran_detecto': {},
    'mapexec': {},
    'obj_verify': {},
    'pmemalloc': {},
    'pmemdetect': {},
    'pmemobjcli': {},
    'pmemspoil': {},
    'pmemwrite': {},
    'usc_permission_check': {},
}

# This dependency is quite rough, but that's the way
# it was in src/tools/Makefile.inc
tools_common = static_library(
    'tools_common',
    objects:[pmempool.extract_objects('pmempool/common.c', 'pmempool/output.c'),
            libpmemblk.extract_objects('btt.c'),
        ],
    install:false
)

tools_common_dep = declare_dependency(
    link_whole: tools_common,
    dependencies: [pmemcommon_dep,
            pmemcore_dep,
        ],
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
