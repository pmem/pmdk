# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2023, Intel Corporation

"""Test framework constants."""

from os.path import join, abspath, dirname

# List of libraries for logging PMDK debug output
LIBS_LIST = ('pmem', 'pmem2', 'pmemobj', 'pmempool')

# Constant paths to repository elements
ROOTDIR = abspath(join(dirname(__file__), '..'))

DEBUG_LIBDIR = abspath(join(ROOTDIR, '..', 'debug'))
NONDEBUG_LIBDIR = abspath(join(ROOTDIR, '..', 'nondebug'))

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
