project(
    'pmdk',
    ['c'],
    license: 'BSD-3-clause',
    version: '1.12.0',
    default_options: [
        'buildtype=debugoptimized',
        'c_std=gnu99',
    ]
)

version_components = meson.project_version().split('.')

pmdk_major_version = version_components[0]
pmdk_minor_version = version_components[1]
pmdk_patch_version = version_components[2]

cc = meson.get_compiler('c')

add_project_arguments(
    '-DSRCVERSION="@0@"'.format(meson.project_version()),
    language: 'c'
)

if get_option('debug')
    add_project_arguments('-DDEBUG', language: 'c')
endif

flags = [
    '-fno-common',
    '-Wconversion',
    '-Wunreachable-code-return',
    '-Wmissing-variable-declarations',
    '-Wfloat-equal',
    '-Wswitch-default',
    '-Wcast-function-type',
    '-Wstringop-truncation',
]

add_project_arguments(cc.get_supported_arguments(flags), language: 'c')

rt_dep = cc.find_library('rt')
threads_dep = dependency('threads')
# TODO: Add wrap (autotools)
libndctl_dep = dependency('libndctl', version: '>= 63', required: get_option('ndctl'))
libdaxctl_dep = dependency('libdaxctl', version: '>= 63', required: get_option('ndctl'))
libfabric_dep = dependency('libfabric', version: '>= 1.4.2')

if libndctl_dep.found()
    add_project_arguments(
        '-DNDCTL_ENABLED=1',
        '-DSDS_ENABLED',
        language: 'c'
    )
endif

subdir('src')
