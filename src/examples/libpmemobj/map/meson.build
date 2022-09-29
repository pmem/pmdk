libmap_ctree = library(
    'map_ctree',
    files('map_ctree.c', 'map.c'),
    dependencies: [libctree_map_dep, libpmemobj_dep, threads_dep],
    include_directories: include_directories('.'),
    install: false,
)

libmap_ctree_dep = declare_dependency(
    link_with: libmap_ctree,
    include_directories: include_directories('.'),
)

libmap = static_library(
    'map',
    files(
	'map.c',
	'map_ctree.c',
	'map_btree.c',
	'map_rtree.c',
	'map_rbtree.c',
	'map_skiplist.c',
	'map_hashmap_atomic.c',
	'map_hashmap_tx.c',
	'map_hashmap_rp.c',
    ),
    dependencies: [
        libpmemobj_dep,
	libctree_map_dep,
        libbtree_map_dep,
        librtree_map_dep,
        librbtree_map_dep,
        libhashmap_atomic_dep,
        libhashmap_tx_dep,
        libhashmap_rp_dep,
        libskiplist_map_dep,
    ],
    include_directories: include_directories,
    install: false,
)

libmap_dep = declare_dependency(
    link_with: libmap,
    include_directories: include_directories('.'),
)

mapcli = executable(
    'mapcli',
    'mapcli.c',
    dependencies: [libpmemobj_dep, threads_dep, libmap_dep],
    include_directories: include_directories,
)

data_store = executable(
    'data_store',
    'data_store.c',
    dependencies: [libpmemobj_dep, threads_dep, libmap_dep],
    include_directories: include_directories,
)

if libuv_dep.found()
    kv_server = executable(
        'kv_server',
	'kv_server.c',
	dependencies: [libpmemobj_dep, threads_dep, libmap_dep, libuv_dep],
	include_directories: include_directories('.'),
    )
else
    message('''Skipping kv_server because libuv library is missing.
See src/examples/libpmemobj/map/README for details.''')
endif
