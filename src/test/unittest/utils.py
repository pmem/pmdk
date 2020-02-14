# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation

"""Utilities for tests"""

import sys
import platform

HEADER_SIZE = 4096

#
# KiB, MiB, GiB ... -- byte unit prefixes
#
KiB = 2 ** 10
MiB = 2 ** 20
GiB = 2 ** 30
TiB = 2 ** 40
PiB = 2 ** 50


def require_architectures(*archs):
    """Enable test only for specified architectures"""
    def wrapped(tc):
        if platform.machine() not in archs:
            tc.enabled = False
        return tc

    return wrapped


def _os_only(tc, os_name):
    """
    Disable test case (TEST[number] class) if NOT run on selected OS.
    Otherwise, the test is not reenabled if it was already disabled
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


def windows_only(tc):
    """
    Disable test case (TEST[number] class) if NOT run on Windows.
    Use it as a class decorator.
    """
    return _os_only(tc, 'win32')


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


def windows_exclude(tc):
    """
    Disable test case (TEST[number] class) on Windows.
    Use it as a class decorator.
    """
    return _os_exclude(tc, 'win32')


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
