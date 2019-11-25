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

"""Granularity context classes and utilities"""

import os
import shutil

import context as ctx
import configurator
import futils


class Granularity(metaclass=ctx.CtxType):
    gran_detecto_arg = None

    def __init__(self, **kwargs):
        futils.set_kwargs_attrs(self, kwargs)
        self.config = configurator.Configurator().config
        self.force = False

    def setup(self, tools=None):
        if not os.path.exists(self.testdir):
            os.makedirs(self.testdir)

        check_page = tools.gran_detecto(self.testdir, self.gran_detecto_arg)
        if not self.force and check_page.returncode != 0:
            msg = check_page.stdout
            detect = tools.gran_detecto(self.testdir, '-d')
            msg = '{}{}{}'.format(os.linesep, msg, detect.stdout)
            raise futils.Fail('Granularity check for {} failed: {}'
                              .format(self.testdir, msg))

    def clean(self, *args, **kwargs):
        shutil.rmtree(self.testdir, ignore_errors=True)

    @classmethod
    def filter(cls, config, msg, tc):
        """
        Acquire file system granularity for the test to be run
        based on configuration and test requirements
        """
        req_gran, kwargs = ctx.get_requirement(tc, 'granularity', None)

        if req_gran == Non:
            return [Non(**kwargs), ]

        if req_gran == CACHELINE_OR_LESS:
            tmp_gran = [Byte, CacheLine]
        else:
            tmp_gran = req_gran

        filtered = ctx.filter_contexts(config.granularity, tmp_gran)

        if req_gran == CACHELINE_OR_LESS:
            if Byte in filtered:
                req_gran = [Byte, ]
            elif CacheLine in filtered:
                req_gran = [CacheLine, ]
            else:
                return []

        kwargs['tc_dirname'] = tc.tc_dirname
        kwargs['force_page'] = config.force_page
        kwargs['force_cacheline'] = config.force_cacheline
        kwargs['force_byte'] = config.force_byte

        gs = []
        for g in filtered:
            try:
                gs.append(g(**kwargs))
            except futils.Skip as s:
                msg.print_verbose('{}: SKIP: {}'.format(tc, s))

        return gs


class Page(Granularity):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.gran_detecto_arg = '-p'
        self.dir = os.path.abspath(self.config.page_fs_dir)
        self.testdir = os.path.join(self.dir, self.tc_dirname)
        self.force = kwargs['force_page']


class CacheLine(Granularity):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.gran_detecto_arg = '-c'
        self.dir = os.path.abspath(self.config.cacheline_fs_dir)
        self.testdir = os.path.join(self.dir, self.tc_dirname)
        self.force = kwargs['force_cacheline']


class Byte(Granularity):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.gran_detecto_arg = '-b'
        self.dir = os.path.abspath(self.config.byte_fs_dir)
        self.testdir = os.path.join(self.dir, self.tc_dirname)
        self.force = kwargs['force_byte']


class Non(Granularity):
    """
    No filesystem is used - granularity is irrelevant to the test.
    To ensure that the test indeed does not use any filesystem,
    accessing some fields of this class is prohibited.
    """
    explicit = True

    def setup(self, tools=None):
        pass

    def cleanup(self, *args, **kwargs):
        pass

    def __getattribute__(self, name):
        if name in ('dir', ):
            raise AttributeError("'{}' attribute cannot be used if the"
                                 " test is meant not to use any "
                                 "filesystem"
                                 .format(name))
        else:
            return super().__getattribute__(name)


# special kind of granularity requirement
CACHELINE_OR_LESS = 'cacheline_or_less'


def require_page_granularity(**kwargs):
    def wrapped(tc):
        ctx.add_requirement(tc, 'granularity', [Page, ], **kwargs)
        return tc
    return wrapped


def require_cacheline_granularity(**kwargs):
    def wrapped(tc):
        ctx.add_requirement(tc, 'granularity', [CacheLine, ], **kwargs)
        return tc
    return wrapped


def require_byte_granularity(**kwargs):
    def wrapped(tc):
        ctx.add_requirement(tc, 'granularity', [Byte, ], **kwargs)
        return tc
    return wrapped


def require_cl_or_less_granularity(**kwargs):
    def wrapped(tc):
        ctx.add_requirement(tc, 'granularity', CACHELINE_OR_LESS, **kwargs)
        return tc
    return wrapped


def require_non_granularity(**kwargs):
    def wrapped(tc):
        ctx.add_requirement(tc, 'granularity', Non, **kwargs)
        return tc
    return wrapped
