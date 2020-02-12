# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

"""
Functionalities for acquiring (through 'filtering' execution configuration and
test specification) set of context parameters with which the test will be run
"""

import sys
import itertools

import builds
import futils
import granularity
import devdax

import context as ctx
import valgrind as vg

if sys.platform != 'win32':
    CTX_TYPES = (vg.Valgrind, granularity.Granularity, devdax.DevDaxes)
else:
    CTX_TYPES = (granularity.Granularity, )


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
