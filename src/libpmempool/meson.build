sources = files(
    'libpmempool.c',
    'check.c',
    'check_bad_blocks.c',
    'check_backup.c',
    'check_btt_info.c',
    'check_log.c',
    'check_blk.c',
    'check_pool_hdr.c',
    'check_sds.c',
    'check_util.c',
    'check_write.c',
    'pool.c',
    'replica.c',
    'feature.c',
    '../rpmem_common/rpmem_common.c',
    '../librpmem/rpmem_ssh.c',
    '../librpmem/rpmem_cmd.c',
    '../librpmem/rpmem_util.c',
    'sync.c',
    'transform.c',
    'rm.c'
)

libpmempool = library(
    'pmempool',
    sources,
    c_args: [
        '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'libpmempool.link.in'),
        '-DUSE_LIBDL',
        '-DUSE_RPMEM',
    ],
    dependencies: [
        threads_dep,
        libdaxctl_dep,
        libndctl_dep,
        pmemcommon_dep,
        pmemcore_dep,
        libpmem_dep,
    ],
    include_directories: [
        root_include,
        include_directories('../libpmemlog'),
        include_directories('../libpmemblk'),
        include_directories('../libpmemobj'),
        include_directories('../rpmem_common'),
        include_directories('../librpmem'),
        include_directories('../libpmem2'),
    ],
    soversion: '1',
    version: '1.0.0',
    install: true
)

libpmempool_dep = declare_dependency(
    link_with: libpmemobj,
    include_directories: include_directories('.')
)

meson.override_dependency('libpmempool', libpmempool_dep)
