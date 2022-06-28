sources = files(
    'util_ravl.c',
)

dependencies = [
    unittest_dep,
]

c_args = []
include_directories = [
    include_directories('../../common'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
