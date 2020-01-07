#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation

# file-exceptions.sh - filter out files not checked for copyright and license

grep -v -E -e '/queue.h$' -e '/getopt.h$' -e '/getopt.c$' -e 'src/core/valgrind/' -e '/testconfig\...$'
