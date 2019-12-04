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
"""Build context classes"""

import sys

import context as ctx
import futils


class Build(metaclass=ctx.CtxType):
    """Base and factory class for standard build classes"""
    exesuffix = ''

    def set_env_common(self):
        if sys.platform == 'win32':
            self.env = {'PATH': self.libdir}
        else:
            self.env = {'LD_LIBRARY_PATH': self.libdir}

    @classmethod
    def filter(cls, config, msg, tc):
        req_builds, kwargs = ctx.get_requirement(tc, 'build', None)

        builds = []
        for b in ctx.filter_contexts(config.build, req_builds):
            try:
                builds.append(b(**kwargs))
            except futils.Skip as s:
                msg.print_verbose('{}: SKIP: {}'.format(tc, s))

        return builds


class Debug(Build):
    """Set this context for a debug build"""
    is_preferred = True

    def __init__(self):
        if sys.platform == 'win32':
            self.exedir = futils.WIN_DEBUG_EXEDIR
        self.libdir = futils.DEBUG_LIBDIR
        self.set_env_common()


class Release(Build):
    """Set this context for a release build"""
    is_preferred = True

    def __init__(self):
        if sys.platform == 'win32':
            self.exedir = futils.WIN_RELEASE_EXEDIR
        self.libdir = futils.RELEASE_LIBDIR
        self.set_env_common()


# Build types not available on Windows
if sys.platform != 'win32':
    class Static_Debug(Build):
        """Set this context for a static_debug build"""

        def __init__(self):
            self.exesuffix = '.static-debug'
            self.libdir = futils.DEBUG_LIBDIR

    class Static_Release(Build):
        """Set this context for a static_release build"""

        def __init__(self):
            self.exesuffix = '.static-nondebug'
            self.libdir = futils.RELEASE_LIBDIR


def require_build(build, **kwargs):
    def wrapped(tc):
        builds = ctx.str_to_ctx_common(build, Build)
        ctx.add_requirement(tc, 'build', builds, **kwargs)
        return tc
    return wrapped
