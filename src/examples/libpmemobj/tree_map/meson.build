libctree_map = static_library(
    'ctree_map',
    'ctree_map.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories,
    install: false,
)

libctree_map_dep = declare_dependency(
    link_with: libctree_map,
    include_directories: include_directories('.'),
)

libbtree_map = static_library(
    'btree_map',
    'btree_map.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories,
    install: false,
)

libbtree_map_dep = declare_dependency(
    link_with: libbtree_map,
    include_directories: include_directories('.'),
)

librtree_map = static_library(
    'rtree_map',
    'rtree_map.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories,
    install: false,
)

librtree_map_dep = declare_dependency(
    link_with: librtree_map,
    include_directories: include_directories('.'),
)

librbtree_map = static_library(
    'rbtree_map',
    'rbtree_map.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories,
    install: false,
)

librbtree_map_dep = declare_dependency(
    link_with: librbtree_map,
    include_directories: include_directories('.'),
)
