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
from enum import Enum, unique

import context as ctx
import configurator
import futils


class Granularity(metaclass=ctx.CtxType):
    gran_detecto_arg, config_dir_field, force_key = None, None, None

    def __init__(self, **kwargs):
        futils.set_kwargs_attrs(self, kwargs)
        self.config = configurator.Configurator().config
        self.dir = os.path.abspath(getattr(self.config, self.config_dir_field))
        self.testdir = os.path.join(self.dir, self.tc_dirname)
        self.force = kwargs[self.force_key]

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
    def testdir_defined(cls, config):
        """
        Check if test directory used by specific granularity setup
        is defined by test configuration
        """
        try:
            getattr(config, cls.config_dir_field)
        except futils.Skip as s:
            msg = futils.Message(config.unittest_log_level)
            msg.print_verbose(s)
            return False
        else:
            return True

    @classmethod
    def filter(cls, config, msg, tc):
        """
        Acquire file system granularity for the test to be run
        based on configuration and test requirements
        """
        req_gran, kwargs = ctx.get_requirement(tc, 'granularity', None)

        if req_gran == Non:
            return [Non(**kwargs), ]

        if req_gran == _CACHELINE_OR_LESS:
            tmp_req_gran = [Byte, CacheLine]
        elif req_gran == _PAGE_OR_LESS:
            tmp_req_gran = [Byte, CacheLine, Page]
        else:
            tmp_req_gran = req_gran

        filtered = ctx.filter_contexts(config.granularity, tmp_req_gran)

        # remove granularities if respective test directories in
        # test config are not defined
        filtered = [f for f in filtered if f.testdir_defined(config)]

        kwargs['tc_dirname'] = tc.tc_dirname
        kwargs['force_page'] = config.force_page
        kwargs['force_cacheline'] = config.force_cacheline
        kwargs['force_byte'] = config.force_byte

        if len(filtered) > 1 and req_gran == _CACHELINE_OR_LESS or \
           req_gran == _PAGE_OR_LESS:

            def order_by_smallest(elem):
                ordered = [Byte, CacheLine, Page]
                return ordered.index(elem)

            # take the smallest available granularity
            filtered.sort(key=order_by_smallest)
            filtered = [filtered[0], ]

        gs = []
        for g in filtered:
            try:
                gs.append(g(**kwargs))
            except futils.Skip as s:
                msg.print_verbose('{}: SKIP: {}'.format(tc, s))

        return gs


class Page(Granularity):
    config_dir_field = 'page_fs_dir'
    force_key = 'force_page'
    gran_detecto_arg = '-p'


class CacheLine(Granularity):
    config_dir_field = 'cacheline_fs_dir'
    force_key = 'force_cacheline'
    gran_detecto_arg = '-c'


class Byte(Granularity):
    config_dir_field = 'byte_fs_dir'
    force_key = 'force_byte'
    gran_detecto_arg = '-b'


class Non(Granularity):
    """
    No filesystem is used - granularity is irrelevant to the test.
    To ensure that the test indeed does not use any filesystem,
    accessing some fields of this class is prohibited.
    """
    explicit = True

    def __init__(self, **kwargs):
        pass

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
_CACHELINE_OR_LESS = 'cacheline_or_less'
_PAGE_OR_LESS = 'page_or_less'


@unique
class _Granularity(Enum):
    PAGE = [Page, ]
    CACHELINE = [CacheLine, ]
    BYTE = [Byte, ]
    CL_OR_LESS = _CACHELINE_OR_LESS
    PAGE_OR_LESS = _PAGE_OR_LESS


PAGE = _Granularity.PAGE
CACHELINE = _Granularity.CACHELINE
BYTE = _Granularity.BYTE
CL_OR_LESS = _Granularity.CL_OR_LESS
PAGE_OR_LESS = _Granularity.PAGE_OR_LESS


def require_granularity(granularity, **kwargs):
    if not isinstance(granularity, _Granularity):
        raise ValueError('selected granularity {} is invalid'
                         .format(granularity))

    def wrapped(tc):
        ctx.add_requirement(tc, 'granularity', granularity.value, **kwargs)
        return tc
    return wrapped


def no_testdir(**kwargs):
    def wrapped(tc):
        ctx.add_requirement(tc, 'granularity', Non, **kwargs)
        return tc
    return wrapped
