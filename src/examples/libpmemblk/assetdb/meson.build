include_directories = [
    include_directories('../../'),
    include_directories('.'),
]

executable(
    'asset_checkin',
    'asset_checkin.c',
    dependencies: [libpmemblk_dep],
    include_directories: include_directories
)

executable(
    'asset_checkout',
    'asset_checkout.c',
    dependencies: [libpmemblk_dep],
    include_directories: include_directories
)

executable(
    'asset_list',
    'asset_list.c',
    dependencies: [libpmemblk_dep],
    include_directories: include_directories
)

executable(
    'asset_load',
    'asset_load.c',
    dependencies: [libpmemblk_dep],
    include_directories: include_directories
)
