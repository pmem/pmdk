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
"""Decorators and functions for controlling test requirements"""

from collections import namedtuple
import sys

import context as ctx
import valgrind as vg


# special kind of granularity requirement
CACHELINE_OR_LESS = 'cacheline_or_less'


def _str_to_ctx_common(val, ctx_base_type):
    def class_from_string(name, base):
        if name == 'all':
            return base.__subclasses__()

        try:
            return next(b for b in base.__subclasses__()
                        if str(b) == name.lower())
        except StopIteration:
            print('Invalid context value: "{}".'.format(name))
            raise

    if isinstance(val, list):
        classes = [class_from_string(cl, ctx_base_type) for cl in val]
        return ctx.expand(*classes)
    else:
        return ctx.expand(class_from_string(val, ctx_base_type))


def require_valgrind_enabled(valgrind):
    def wrapped(tc):
        if sys.platform == 'win32':
            # do not run valgrind tests on windows
            tc.enabled = False
            return tc

        tool = _require_valgrind_common(valgrind)
        setattr(tc, '_required_enabled_valgrind', tool)

        return tc

    return wrapped


def require_valgrind_disabled(valgrind):
    def wrapped(tc):
        disabled_tools = []
        if isinstance(valgrind, list):
            for v in valgrind:
                disabled_tools.append(_require_valgrind_common(v))
        else:
            disabled_tools.append(_require_valgrind_common(valgrind))

        setattr(tc, '_required_disabled_valgrind', disabled_tools)

        return tc

    return wrapped


def _require_valgrind_common(v):
    valid_tool_names = [str(t) for t in vg.TOOLS]
    if v not in valid_tool_names:
        sys.exit('used name {} not in valid valgrind tool names which are: {}'
                 .format(v, valid_tool_names))

    str_to_tool = next(t for t in vg.TOOLS if v == str(t))
    return str_to_tool


def require_build(build, **kwargs):
    def wrapped(tc):
        tc._required_build = _str_to_ctx_common(build, ctx._Build)
        tc._required_build_kwargs = kwargs
        return tc
    return wrapped


def require_fs(fs, **kwargs):
    def wrapped(tc):
        tc._required_fs = _str_to_ctx_common(fs, ctx._Fs)
        tc._required_fs_kwargs = kwargs
        return tc
    return wrapped


def require_page_granularity(**kwargs):
    def wrapped(tc):
        tc._required_gran = [ctx.Page, ]
        tc._required_gran_kwargs = kwargs

        return tc
    return wrapped


def require_cacheline_granularity(**kwargs):
    def wrapped(tc):
        tc._required_gran = [ctx.CacheLine, ]
        tc._required_gran_kwargs = kwargs
        return tc
    return wrapped


def require_byte_granularity(**kwargs):
    def wrapped(tc):
        tc._required_gran = [ctx.Byte, ]
        tc._required_gran_kwargs = kwargs
        return tc
    return wrapped



def require_cl_or_less_granularity(**kwargs):
    def wrapped(tc):
        tc._required_gran = CACHELINE_OR_LESS
        tc._required_gran_kwargs = kwargs
        return tc
    return wrapped


def require_non_granularity(**kwargs):
    """The test uses no filesystem"""
    def wrapped(tc):
        tc._required_gran = ctx.Non
        tc._required_gran_kwargs = kwargs
        return tc
    return wrapped


def get_requirements(tc):
    rqs = {}

    rqs['build'] = getattr(tc, '_required_build', None)
    rqs['build_kwargs'] = getattr(tc, '_required_build_kwargs', {})

    rqs['fs'] = getattr(tc, '_required_fs', None)
    rqs['fs_kwargs'] = getattr(tc, '_required_fs_kwargs', {})

    rqs['granularity'] = getattr(tc, '_required_gran', None)
    rqs['granularity_kwargs'] = getattr(tc, '_required_gran_kwargs', {})

    rqs['enabled_valgrind'] = getattr(tc, '_required_enabled_valgrind',
                                      vg._Tool.NONE)
    rqs['enabled_valgrind_kwargs'] = \
        getattr(tc, '_required_enabled_valgrind_kwargs', {})

    rqs['disabled_valgrind'] = getattr(tc, '_required_disabled_valgrind',
                                       [vg._Tool.NONE, ])
    rqs['disabled_valgrind_kwargs'] = \
        getattr(tc, '_required_disabled_valgrind_kwargs', {})
    return namedtuple('Requirements', rqs.keys())(*rqs.values())
