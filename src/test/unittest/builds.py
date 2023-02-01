# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation
#
"""Build context classes"""

import context as ctx
import futils
import consts as c


class Build(metaclass=ctx.CtxType):
    """Base and factory class for standard build classes"""
    exesuffix = ''


    @classmethod
    def filter(cls, config, msg, tc):
        req_builds, kwargs = ctx.get_requirement(tc, 'build', None)

        builds = []
        for b in ctx.filter_contexts(config.build, req_builds):
            try:
                builds.append(b(**kwargs))
            except futils.Skip as s:
                msg.print('{}: SKIP: {}'.format(tc, s))

        return builds


class Debug(Build):
    """Set this context for a debug build"""
    is_preferred = True

    def __init__(self):
        self.libdir = c.DEBUG_LIBDIR
        self.env = {'LD_LIBRARY_PATH': self.libdir}


class Release(Build):
    """Set this context for a release build"""
    is_preferred = True

    def __init__(self):
        self.libdir = c.RELEASE_LIBDIR
        self.env = {'LD_LIBRARY_PATH': self.libdir}


class Static_Debug(Build):
    """Set this context for a static_debug build"""

    def __init__(self):
        self.exesuffix = '.static-debug'
        self.libdir = c.DEBUG_LIBDIR


class Static_Release(Build):
    """Set this context for a static_release build"""

    def __init__(self):
        self.exesuffix = '.static-nondebug'
        self.libdir = c.RELEASE_LIBDIR


def require_build(build, **kwargs):
    def wrapped(tc):
        builds = ctx.str_to_ctx_common(build, Build)
        ctx.add_requirement(tc, 'build', builds, **kwargs)
        return tc
    return wrapped
