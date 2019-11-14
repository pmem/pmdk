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
"""External tools integration"""


import os
import sys
import subprocess as sp

import futils

try:
    import envconfig
    envconfig = envconfig.config
except ImportError:
    # if file doesn't exist create dummy object
    envconfig = {'GLOBAL_LIB_PATH': ''}

PMEMDETECT_FALSE = 0
PMEMDETECT_TRUE = 1
PMEMDETECT_ERROR = 2


def pmemdetect(ctx, *args):
    env = os.environ.copy()

    if sys.platform == 'win32':
        env['PATH'] = envconfig['GLOBAL_LIB_PATH'] + os.pathsep +\
            ctx.build.libdir + os.pathsep +\
            env.get('PATH', '')
    else:
        env['LD_LIBRARY_PATH'] = envconfig['GLOBAL_LIB_PATH'] + os.pathsep +\
            ctx.build.libdir + os.pathsep +\
            env.get('LD_LIBRARY_PATH', '')

    exe = futils.get_test_tool_path(ctx, 'pmemdetect')

    return sp.run([exe, *args], env=env, stdout=sp.PIPE,
                  stderr=sp.STDOUT, universal_newlines=True)
