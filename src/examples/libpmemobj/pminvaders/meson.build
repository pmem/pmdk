# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

if libncurses_dep.found()
    executable(
        'pminvaders',
        'pminvaders.c',
        dependencies: [libpmemobj_dep, libpmem_dep, threads_dep, libncurses_dep],
        include_directories: include_directories('.'),
    )

    executable(
        'pminvaders2',
        'pminvaders2.c',
        dependencies: [libpmemobj_dep, libpmem_dep, threads_dep, libncurses_dep],
        include_directories: include_directories('.'),
    )
else
    message('''Skipping pminvaders and pminvaders2 because ncurses library is missing.
See src/examples/libpmemobj/pminvaders/README for details.''')
endif

