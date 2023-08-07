# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation
#

"""Granularity context classes and utilities"""

import os
import shutil
from enum import Enum, unique

import context as ctx
import configurator
import futils

from tools import Tools
from tools import Ndctl


class Granularity(metaclass=ctx.CtxType):
    """
    Represents file system granularity context element.

    Attributes:
        gran_detecto_arg (str): argument of gran_detecto tool
            that checks test directory for this granularity
            type compliance
        config_dir_field (str): name of the field in testconfig
            that represents the test directory with this
            granularity type
        config_force_field (str): name of the field in testconfig
            that represent the 'force' argument for this
            granularity type
        force_env (str): value of PMEM2_FORCE_GRANULARITY environment
            variable for this granularity type
        pmem_force_env (str): value for legacy PMEM_IS_PMEM_FORCE
            environment variable that corresponds to this granularity
            type

    """

    gran_detecto_arg = None
    config_dir_field = None
    config_force_field = None
    force_env = None
    pmem_force_env = None

    def __init__(self, **kwargs):
        self._env = {}
        self.tools = Tools(self._env, None)
        futils.set_kwargs_attrs(self, kwargs)
        self.config = configurator.Configurator().config
        self.dir_ = os.path.abspath(getattr(self.config,
                                            self.config_dir_field))
        self.testdir = os.path.join(self.dir_, self.tc_dirname)
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

    def _check_real_pmem_req_is_met(self, tc):
        req_real_pmem, _ = ctx.get_requirement(tc, 'require_real_pmem', False)
        if not req_real_pmem:
            return True

        devdaxes, _ = ctx.get_requirement(tc, 'require_devdax', None)
        # devdax cases should be dealt in devdax module
        if devdaxes is not None:
            return True

        if Ndctl().is_emulated(self.dir_):
            raise futils.Skip('skip emulated pmem')

        return True

    def _check_usc_req_is_met(self, tc):
        require_usc, _ = ctx.get_requirement(tc, 'require_usc', False)
        if not require_usc:
            return True

        basedir = self.testdir.replace(self.testdir, '')
        filepath = os.path.join(basedir, "__usc_test_file")
        f = open(filepath, 'w')
        f.close()

        check = self.tools.usc_permission_check(filepath)
        usc_available = check.returncode == 0

        os.remove(filepath)

        if not usc_available:
            raise futils.Skip('unsafe shutdown count is not available')

        return usc_available

    @classmethod
    def filter(cls, config, msg, tc):
        """
        Initialize granularity classes for the test to be run
        based on configuration and test requirements.

        Args:
            config: configuration as returned by Configurator class
            msg (Message): level based logger class instance
            tc (BaseTest): test case, from which the granularity
                requirements are obtained

        Returns:
            list of granularities on which the test should be run

        """
        req_gran, kwargs = ctx.get_requirement(tc, 'granularity', None)

        # remove granularities if respective test directories in
        # test config are not defined
        conf_defined = [c for c in config.granularity
                        if c.testdir_defined(config)]

        if req_gran == Non:
            return [Non(**kwargs), ]

        if req_gran == [_CACHELINE_OR_LESS]:
            tmp_req_gran = [Byte, CacheLine]
        elif req_gran == [_PAGE_OR_LESS]:
            tmp_req_gran = [Byte, CacheLine, Page]
        elif req_gran == [ctx.Any, ]:
            tmp_req_gran = ctx.Any.get(conf_defined)
        else:
            tmp_req_gran = req_gran

        filtered = ctx.filter_contexts(conf_defined, tmp_req_gran)

        kwargs['tc_dirname'] = tc.tc_dirname

        if len(filtered) > 1 and req_gran == [_CACHELINE_OR_LESS] or \
           req_gran == [_PAGE_OR_LESS]:

            def order_by_smallest(elem):
                ordered = [Byte, CacheLine, Page]
                return ordered.index(elem)

            # take the smallest available granularity
            filtered.sort(key=order_by_smallest)
            filtered = [filtered[0], ]

        gs = []
        for g in filtered:
            try:
                gran = g(**kwargs)
                gran._check_usc_req_is_met(tc)
                gran._check_real_pmem_req_is_met(tc)
                gs.append(gran)
            except futils.Skip as s:
                msg.print('{}: SKIP: {}'.format(tc, s))

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


def require_usc(**kwargs):
    def wrapped(tc):
        ctx.add_requirement(tc, 'require_usc', True)
        return tc
    return wrapped


def require_real_pmem(**kwargs):
    def wrapped(tc):
        ctx.add_requirement(tc, 'require_real_pmem', True)
        return tc
    return wrapped
