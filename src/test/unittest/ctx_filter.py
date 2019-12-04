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

"""
Functionalities for acquiring (through 'filtering' execution configuration and
test specification) set of context parameters with which the test will be run
"""

import sys
import itertools

import builds
import futils
import filesystems
import context as ctx
import valgrind as vg

if sys.platform != 'win32':
    CTX_TYPES = (vg.Valgrind, filesystems.Fs)
else:
    CTX_TYPES = (filesystems.Fs, )


class CtxFilter:
    """
    Generates contexts based on provided
    configuration and test requirements
    """

    def __init__(self, config, tc):
        self.tc = tc
        self.config = config
        self.msg = futils.Message(config.unittest_log_level)

    def _get_builds(self):
        return builds.Build.filter(self.config, self.msg, self.tc)

    def get_contexts(self):
        """
        Generate a list of context based on configuration and
        test requirements
        """
        params = self._get_configurational_params()
        builds = self._get_builds()

        ctx_params = itertools.product(builds, *params.values())
        ctxs = []
        for cp in ctx_params:
            build = cp[0]
            kwargs = dict(zip(params.keys(), cp[1:]))
            c = ctx.Context(build, **kwargs)
            c.cwd = self.tc.cwd
            ctxs.append(c)
        return ctxs

    def _get_configurational_params(self):
        """
        Get special test parameters, like file system or valgrind tool
        which final content depends on test requirements, user configuration
        and/or provided test plan.
        """
        params = {}
        for param in CTX_TYPES:
            params[param.__name__.lower()] = \
                param.filter(self.config, self.msg, self.tc)
        return params
