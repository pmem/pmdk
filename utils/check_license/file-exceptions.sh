#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2023, Intel Corporation

# file-exceptions.sh - filter out files not checked for copyright and license

grep -v -E -e '/queue.h$' -e 'src/core/valgrind/' -e '/testconfig\...$' \
	 -e'src/deps/miniasync/LICENSE'
