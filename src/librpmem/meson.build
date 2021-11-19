sources = files(
    '../core/alloc.c',
    '../core/os_posix.c',
    '../core/os_thread_posix.c',
    '../core/out.c',
    '../core/util.c',
    '../core/util_posix.c',
    'librpmem.c',
    'rpmem.c',
    'rpmem_obc.c',
    'rpmem_cmd.c',
    'rpmem_ssh.c',
    '../rpmem_common/rpmem_common.c',
    'rpmem_util.c',
    '../rpmem_common/rpmem_fip_common.c',
    'rpmem_fip.c'
)

librpmem = library(
    'rpmem',
    sources,
    c_args: [
        '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'librpmem.link.in'),
        '-DRPMEMC_LOG_RPMEM',
    ],
    dependencies: [
        threads_dep,
        libfabric_dep,
    ],
    include_directories: [
        root_include,
        core_include,
        common_include,
        include_directories('../rpmem_common'),
    ],
    soversion: '1',
    version: '1.0.0',
    install: true
)

librpmem_dep = declare_dependency(
    link_with: librpmem,
    include_directories: include_directories('.')
)

meson.override_dependency('librpmem', librpmem_dep)
