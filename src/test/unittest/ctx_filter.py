# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation

"""
Functionalities for acquiring (through 'filtering' execution configuration and
test specification) set of context parameters with which the test will be run.
"""

import sys
import itertools

import builds
import futils
import granularity
import devdax

import context as ctx
import valgrind as vg
import requirements as req

if sys.platform != 'win32':
    CTX_TYPES = (vg.Valgrind, granularity.Granularity, devdax.DevDaxes)
else:
    CTX_TYPES = (granularity.Granularity, )


class CtxFilter:
    """
    Generates contexts based on provided
    configuration and test requirements.

    Attributes:
        tc (BaseTest): test using the context
        config: configuration class
        msg (Message): level based logger
    """

    def __init__(self, config, tc):
        self.tc = tc
        self.config = config
        self.msg = futils.Message(config.unittest_log_level)

    def _get_builds(self):
        return builds.Build.filter(self.config, self.msg, self.tc)

    def get_contexts(self):
        """
        Generate a list of contexts based on configuration and
        test requirements
        """
        reqs = req.Requirements()
        if not reqs.check_if_all_requirements_are_met(self.tc):
            return []
        conf_params = self._get_configured_params()
        common_params = self._get_common_params()
        ctx_elem_names = list(conf_params.keys()) + list(common_params.keys())

        builds = self._get_builds()

        # Generate cartesian product of builds and context elements.
        # Each element of the product serves as a base for the separate
        # context in which the test is run
        ctx_params = itertools.product(builds, *conf_params.values(),
                                       *common_params.values())
        ctxs = []
        for cp in ctx_params:
            build = cp[0]

            ctx_elems = dict(zip(ctx_elem_names, cp[1:]))
            c = ctx.Context(build, **ctx_elems)
            c.cwd = self.tc.cwd
            ctxs.append(c)

        return ctxs

    def _get_configured_params(self):
        """
        Get special test parameters, like file system or Valgrind tool
        which final content depends on test requirements, and user
        configuration. This content is obtained through its
        static filter() method.
        """

        # TODO: instead of using a hard-coded CTX_TYPES list,
        # detect configured parameters based on whether they
        # implement the filter() method.
        # Configured parameter therefore becomes
        # the specific case of common parameter
        # and may be implemented ad-hoc by the test developer

        params = {}
        for param in CTX_TYPES:
            params[param.__name__.lower()] = \
                param.filter(self.config, self.msg, self.tc)
        return params

    def _get_common_params(self):
        """
        Get generic test params, added through ctx.add_params() function
        """
        return ctx.get_params(self.tc)
