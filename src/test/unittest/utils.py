# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation

"""Utilities for tests. Meant to be used by test user."""

import sys
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


def _os_only(tc, os_name):
    """
    Disable test case (TEST[number] class) if NOT run on selected OS.
    Otherwise, the test is not re-enabled if it was already disabled
    elsewhere.
    Internal helper function.
    """
    if not sys.platform.startswith(os_name):
        tc.enabled = False
    return tc


def _os_exclude(tc, os_name):
    """
    Disable test case (TEST[number] class) on selected OS.
    Internal helper function.
    """
    if sys.platform.startswith(os_name):
        tc.enabled = False
    return tc


def linux_only(tc):
    """
    Disable test case (TEST[number] class) if NOT run on Linux.
    Use it as a class decorator.
    """
    return _os_only(tc, 'linux')


def freebsd_only(tc):
    """
    Disable test case (TEST[number] class) if NOT run on FreeBSD.
    Use it as a class decorator.
    """
    return _os_only(tc, 'freebsd')


def linux_exclude(tc):
    """
    Disable test case (TEST[number] class) on Linux.
    Use it as a class decorator.
    """
    return _os_exclude(tc, 'linux')


def freebsd_exclude(tc):
    """
    Disable test case (TEST[number] class) on FreeBSD.
    Use it as a class decorator.
    """
    return _os_exclude(tc, 'freebsd')
