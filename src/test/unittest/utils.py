#
# Copyright 2019, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

"""Utilities for tests"""

import sys
import testconfig

HEADER_SIZE = 4096

#
# KiB, MiB, GiB ... -- byte unit prefixes
#
KiB = 2 ** 10
MiB = 2 ** 20
GiB = 2 ** 30
TiB = 2 ** 40
PiB = 2 ** 50


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

def experimental(tc):
    """
    Disable test case (TEST[number] class) if EXPERIMENTAL flag is equel false.
    Use it as a class decorator.
    """
    if not sys.platform.startswith('win32'):
        if testconfig.config['experimental'] == False:
            tc.enabled = False
    return tc
