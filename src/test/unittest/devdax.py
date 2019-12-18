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

"""Device dax context classes and utilities"""

import os
import sys

import context as ctx
import futils
import tools


class DevDax():
    """
    Class representing dax device and its parameters
    """
    def __init__(self, name):
        self.name = name
        self.path = None

    def __str__(self):
        return self.name


class DevDaxes():
    """
    Dax device context class representing a set of dax devices required
    for test
    """
    def __init__(self, *dax_devices):
        self.dax_devices = tuple(dax_devices)
        for dd in dax_devices:
            setattr(self, dd.name, dd)

    def setup(self, **kwargs):
        tools = kwargs['tools']
        for dd in self.dax_devices:
            proc = tools.pmemdetect('-d', dd.path)
            if proc.returncode != 0:
                raise futils.Fail('checking {} with pmemdetect failed:{}{}'
                                  .format(dd.path, os.linesep, proc.stdout))

    def __str__(self):
        return 'devdax'

    @classmethod
    def filter(cls, config, msg, tc):

        dax_devices, _ = ctx.get_requirement(tc, 'devdax', ())
        if not dax_devices:
            return ctx.NO_CONTEXT
        elif sys.platform == 'win32' and tc.enabled:
            raise futils.Fail('dax device functionality required by "{}" is '
                              'not available on Windows. Please disable the '
                              'test for this platform'.format(tc))

        if not config.device_dax_path:
            raise futils.Skip('No dax devices defined in testconfig')

        if len(dax_devices) > len(config.device_dax_path):
            raise futils.Skip('Not enough dax devices defined in testconfig '
                              '({} needed)'.format(len(dax_devices)))

        ndctl = tools.Ndctl()
        for dd, cddp in zip(dax_devices, config.device_dax_path):
            dd.path = cddp
            if not ndctl.is_devdax(cddp):
                raise futils.Fail('{} is not a dax device'.format(cddp))
            dd.size = ndctl.get_dev_size(cddp)
            dd.alignment = ndctl.get_dev_alignment(cddp)

        return [DevDaxes(*dax_devices), ]


def require_devdax(*dax_devices, **kwargs):
    def wrapped(tc):
        ctx.add_requirement(tc, 'devdax', tuple(dax_devices), **kwargs)
        return tc
    return wrapped
