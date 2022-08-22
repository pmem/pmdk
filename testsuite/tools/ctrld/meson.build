sources = files(
    'ctrld.c',
)

dependencies = [
    pmemcore_dep,
]

c_args = []
include_directories = [
    core_include,
]

executable(
    ttool,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
