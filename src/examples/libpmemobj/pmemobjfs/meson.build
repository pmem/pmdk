# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

FUSE_REQ_VER = '2.9.1'

if libfuse_dep.found()
executable(
    'pmemobjfs',
    'pmemobjfs.c',
    c_args: ['-DFUSE_USE_VERSION=28', '-DUSE_CTREE'],
    dependencies: [libpmemobj_dep, libpmem_dep, threads_dep, libmap_ctree_dep, libfuse_dep],
    include_directories: include_directories,
)
else
    message(f'''Skipping pmemobjfs because fuse library (version >= @FUSE_REQ_VER@) is missing.
See src/examples/libpmemobj/pmemobjfs/README for details.''')
endif