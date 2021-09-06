# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

"""Test framework constants."""

from os.path import join, abspath, dirname
import sys

# List of libraries for logging PMDK debug output
LIBS_LIST = ('pmem', 'pmem2', 'pmemobj', 'pmemblk', 'pmemlog', 'pmempool')

# Constant paths to repository elements
ROOTDIR = abspath(join(dirname(__file__), '..'))

WIN_DEBUG_BUILDDIR = abspath(join(ROOTDIR, '..', 'x64', 'Debug'))
WIN_DEBUG_EXEDIR = abspath(join(WIN_DEBUG_BUILDDIR, 'tests'))

WIN_RELEASE_BUILDDIR = abspath(join(ROOTDIR, '..', 'x64', 'Release'))
WIN_RELEASE_EXEDIR = abspath(join(WIN_RELEASE_BUILDDIR, 'tests'))

if sys.platform == 'win32':
    DEBUG_LIBDIR = abspath(join(WIN_DEBUG_BUILDDIR, 'libs'))
    RELEASE_LIBDIR = abspath(join(WIN_RELEASE_BUILDDIR, 'libs'))
else:
    DEBUG_LIBDIR = abspath(join(ROOTDIR, '..', 'debug'))
    RELEASE_LIBDIR = abspath(join(ROOTDIR, '..', 'nondebug'))

HEADER_SIZE = 4096

#
# KiB, MiB, GiB ... -- byte unit prefixes
#
KiB = 2 ** 10
MiB = 2 ** 20
GiB = 2 ** 30
TiB = 2 ** 40
PiB = 2 ** 50


# platform.machine() used for detecting architectures
# gives different values for different systems.
# Key is a normalized value of possible returned architecture values.
NORMALIZED_ARCHS = {
    'x86_64': ('AMD64', 'x86_64'),
    'arm64': ('arm64', 'aarch64'),
    'ppc64el': ('ppc64el', 'ppc64le')
}
