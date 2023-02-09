# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation

"""Utilities for tests. Meant to be used by test user."""

import platform

import consts as c


def require_architectures(*archs):
    """Enable test only for specified architectures"""
    def wrapped(tc):
        this_arch = platform.machine()

        # normalize this_arch value
        for normalized, possible in c.NORMALIZED_ARCHS.items():
            if this_arch in possible:
                this_arch = normalized

        if this_arch not in archs:
            tc.enabled = False
        return tc

    return wrapped
