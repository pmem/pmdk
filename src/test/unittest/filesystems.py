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
"""Filesystem context classes"""

import os
import shutil

import configurator
import context as ctx
import futils


class Fs(metaclass=ctx.CtxType):
    """Base class for filesystem classes"""

    def __init__(self, **kwargs):
        futils.set_kwargs_attrs(self, kwargs)
        self.conf = configurator.Configurator().config

    def setup(self, *args, **kwargs):
        if not os.path.exists(self.testdir):
            os.makedirs(self.testdir)

    def clean(self, *args, **kwargs):
        shutil.rmtree(self.testdir, ignore_errors=True)

    @classmethod
    def filter(cls, config, msg, tc):
        req_fs, kwargs = ctx.get_requirement(tc, 'fs', None)
        kwargs['tc_dirname'] = tc.tc_dirname

        if req_fs == Non:
            return [Non(**kwargs), ]
        else:
            fss = []
            for f in ctx.filter_contexts(config.fs, req_fs):
                try:
                    fss.append(f(**kwargs))
                except futils.Skip as s:
                    msg.print_verbose('{}: SKIP: {}'.format(tc, s))

            return fss


class Pmem(Fs):
    """Set the context for pmem filesystem"""
    is_preferred = True

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.dir = os.path.abspath(self.conf.pmem_fs_dir)
        self.testdir = os.path.join(self.dir, self.tc_dirname)

        if self.conf.fs_dir_force_pmem == 1:
            self.env = {'PMEM_IS_PMEM_FORCE': '1'}


class Nonpmem(Fs):
    """Set the context for nonpmem filesystem"""
    pass

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.dir = os.path.abspath(self.conf.non_pmem_fs_dir)
        self.testdir = os.path.join(self.dir, self.tc_dirname)


class Non(Fs):
    """
    No filesystem is used. Accessing some fields of this class is prohibited.
    """
    explicit = True

    def __init__(self, **kwargs):
        pass

    def setup(self, *args, **kwargs):
        pass

    def cleanup(self, *args, **kwargs):
        pass

    def __getattribute__(self, name):
        if name in ('dir',):
            raise AttributeError("fs '{}' attribute cannot be used for '{}' fs"
                                 .format(name, self))
        else:
            return super().__getattribute__(name)


def require_fs(fs, **kwargs):
    def wrapped(tc):
        req = ctx.str_to_ctx_common(fs, Fs)
        ctx.add_requirement(tc, 'fs', req, **kwargs)
        return tc
    return wrapped
