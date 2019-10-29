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

import sys
import itertools

import futils
import requirements
import context as ctx
import valgrind as vg


class CtxFilter:
    """
    Generates contexts based on provided
    configuration and test requirements
    """

    def __init__(self, config, tc):
        self.reqs = requirements.get_requirements(tc)
        self.tc = tc
        self.config = config

    def filter_contexts(self, config_ctx, test_ctx):
        """
        Return contexts that should be used in execution based on
        contexts provided by config and test case
        """
        if not test_ctx:
            return [c for c in config_ctx if not c.explicit]
        return [c for c in config_ctx if c in test_ctx]

    def get_contexts(self):
        """
        Generate a list of context based on configuration and
        test requirements
        """

        params = self.get_configurational_params()

        # test_types = filter_test_type(config, requirements)

        ctx_params = itertools.product(*params.values())
        ctxs = []
        for cp in ctx_params:
            kwargs = dict(zip(params.keys(), cp))
            c = ctx.Context(**kwargs)
            c.cwd = self.tc.cwd
            ctxs.append(c)
        return ctxs

    def get_configurational_params(self):
        """
        Get special test parameters which final content depends on test
        requirements, user configuration and/or provided test plan.
        """
        params = {}

        params['build'] = self.filter_build()
        params['fs'] = self.filter_fs()
        if sys.platform != 'win32':
            params['valgrind'] = self.filter_valgrind()
        return params

    def filter_build(self):
        kwargs = self.reqs.build_kwargs
        bs = self.filter_contexts(self.config.build, self.reqs.build)
        return [b(**kwargs) for b in self.filter_contexts(self.config.build,
                                                  self.reqs.build)]

    def filter_fs(self):
        kwargs = self.reqs.fs_kwargs
        kwargs['tc_dirname'] = self.tc.tc_dirname

        if self.reqs.fs == ctx.Non:
            return [ctx.Non(**kwargs), ]
        else:
            fs = self.filter_contexts(self.config.fs, self.reqs.fs)
            return [f(**kwargs) for f in self.filter_contexts(self.config.fs,
                                                      self.reqs.fs)]

    def filter_test_type(self, config, requirements):
        return self.filter_contexts(config.test_type, requirements.test_type)

    def filter_valgrind(self):
        vg_tool = self.reqs.enabled_valgrind
        kwargs = self.reqs.enabled_valgrind_kwargs

        if self.config.force_enable:
            if vg_tool and vg_tool != self.config.force_enable:
                raise futils.Skip(
                    "test enables the '{}' Valgrind tool while "
                    "execution configuration forces '{}'"
                    .format(vg_tool, self.config.force_enable))

            elif self.config.force_enable in requirements.disabled_valgrind:
                raise futils.Skip(
                      "forced Valgrind tool '{}' is disabled by test"
                      .format(self.config.force_enable))

            else:
                vg_tool = self.config.force_enable

        return [vg.Valgrind(vg_tool, self.tc.cwd, self.tc.testnum, **kwargs), ]
