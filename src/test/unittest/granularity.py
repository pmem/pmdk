# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#

"""Granularity context classes and utilities"""

import os
import shutil
from enum import Enum, unique

import context as ctx
import configurator
import futils


class Granularity(metaclass=ctx.CtxType):
    gran_detecto_arg = None
    config_dir_field = None
    config_force_field = None
    force_env = None
    pmem_force_env = None

    def __init__(self, **kwargs):
        futils.set_kwargs_attrs(self, kwargs)
        self.config = configurator.Configurator().config
        dir_ = os.path.abspath(getattr(self.config, self.config_dir_field))
        self.testdir = os.path.join(dir_, self.tc_dirname)
        force = getattr(self.config, self.config_force_field)
        if force:
            self.env = {
                        'PMEM2_FORCE_GRANULARITY': self.force_env,

                        # PMEM2_FORCE_GRANULARITY is implemented only by
                        # libpmem2. Corresponding PMEM_IS_PMEM_FORCE variable
                        # is set to support tests for older PMDK libraries.
                        'PMEM_IS_PMEM_FORCE': self.pmem_force_env
                        }

    def setup(self, tools=None):
        if not os.path.exists(self.testdir):
            os.makedirs(self.testdir)

        check_page = tools.gran_detecto(self.testdir, self.gran_detecto_arg)
        if check_page.returncode != 0:
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
            if not cls.config_dir_field:
                return False
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

        # remove granularities if respective test directories in
        # test config are not defined
        conf_defined = [c for c in config.granularity
                        if c.testdir_defined(config)]

        if req_gran == Non:
            return [Non(**kwargs), ]

        if req_gran == _CACHELINE_OR_LESS:
            tmp_req_gran = [Byte, CacheLine]
        elif req_gran == _PAGE_OR_LESS:
            tmp_req_gran = [Byte, CacheLine, Page]
        elif req_gran == [ctx.Any, ]:
            tmp_req_gran = ctx.Any.get(conf_defined)
        else:
            tmp_req_gran = req_gran

        filtered = ctx.filter_contexts(conf_defined, tmp_req_gran)

        kwargs['tc_dirname'] = tc.tc_dirname

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
    config_force_field = 'force_page'
    force_env = 'PAGE'
    pmem_force_env = '0'
    gran_detecto_arg = '-p'


class CacheLine(Granularity):
    config_dir_field = 'cacheline_fs_dir'
    config_force_field = 'force_cacheline'
    force_env = 'CACHE_LINE'
    pmem_force_env = '1'
    gran_detecto_arg = '-c'


class Byte(Granularity):
    config_dir_field = 'byte_fs_dir'
    config_force_field = 'force_byte'
    force_env = 'BYTE'
    pmem_force_env = '1'
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
    PAGE = Page
    CACHELINE = CacheLine
    BYTE = Byte
    CL_OR_LESS = _CACHELINE_OR_LESS
    PAGE_OR_LESS = _PAGE_OR_LESS
    ANY = ctx.Any


PAGE = _Granularity.PAGE
CACHELINE = _Granularity.CACHELINE
BYTE = _Granularity.BYTE
CL_OR_LESS = _Granularity.CL_OR_LESS
PAGE_OR_LESS = _Granularity.PAGE_OR_LESS
ANY = _Granularity.ANY


def require_granularity(*granularity, **kwargs):
    for g in granularity:
        if not isinstance(g, _Granularity):
            raise ValueError('selected granularity {} is invalid'
                             .format(g))

    enum_values = [g.value for g in granularity]

    def wrapped(tc):
        ctx.add_requirement(tc, 'granularity', enum_values, **kwargs)
        return tc
    return wrapped


def no_testdir(**kwargs):
    def wrapped(tc):
        ctx.add_requirement(tc, 'granularity', Non, **kwargs)
        return tc
    return wrapped
