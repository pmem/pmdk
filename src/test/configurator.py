#
# Copyright 2018, Intel Corporation
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


import argparse
import os
import sys
from datetime import timedelta
from collections import namedtuple

import context as ctx

try:
    import testconfig
except ImportError as e:
    print('Please add valid testconfig.py file - see testconfig.py.example')
    raise e

DEFAULT_CONFIG_VALUES = {
    'unittest_log_level': 2,
    'test_type': 'Check',
    'fs_type': 'allfs',
    'build_type': 'Debug',
    'suffix': '',
    'keep_going': False,
    'rootdir': os.path.abspath(os.path.dirname(__file__))
}


def str2list(config):
    """
    str2list -- converts the string to list
    example:
    str2list("0-3,6") -->  [0, 1, 2, 3, 6]
    str2list("1,3-5") -->  [1, 3, 4, 5]
    """
    arg = config['test_sequence']
    if not arg:
        # test_sequence not set, do nothing
        return

    try:
        seq = []
        if ',' in arg or '-' in arg:
            arg = arg.split(',')
            for word in arg:
                if '-' in word:
                    word = word.split('-')
                    begin = int(word[0])
                    end = int(word[1])
                    step = 1 if begin < end else -1
                    for x in range(begin, end + step, step):
                        seq.append(x)
                else:
                    seq.append(int(word))
        else:
            seq.append(int(arg))

    except (ValueError, IndexError):
        print('Provided test sequence {} is invalid'.format(arg))
        raise

    config['test_sequence'] = seq


def str2time(config):
    """
    str2time -- converts the string with s, m, h, d suffixes to time format

    example:
    str2time("5s")  -->  "0:00:05"
    str2time("15m") -->  "0:15:00"
    """
    string = config['test_timeout']
    try:
        timeout = int(string[:-1])
    except ValueError as e:
        raise ValueError("invalid timeout argument: {}".format(string)) from e
    else:
        if "d" in string:
            timeout = timedelta(days=timeout)
        if "m" in string:
            timeout = timedelta(minutes=timeout)
        if "h" in string:
            timeout = timedelta(hours=timeout)
        if "s" in string:
            timeout = timedelta(seconds=timeout)

        config['test_timeout'] = timeout


def str2ctx(config):
    """ Convert context classes from strings to actual classes """
    def class_from_string(name, base):
        return [b for b in base.__subclasses__()
                if b.__name__.lower() == name.lower()][0]

    def convert_internal(key, base):
        if not isinstance(config[key], list):
            config[key] = [config[key]]
        for i, val in enumerate(config[key]):
            config[key][i] = class_from_string(val, base)

    convert_internal('build_type', ctx.Build)
    convert_internal('test_type', ctx.TestType)
    convert_internal('fs_type', ctx.Fs)


class Configurator():
    def __init__(self):
        self.argparser = self.init_argparser()

    def parse_config(self):
        """
        Parse and return test execution config object. Final config is
        composed from 3 config values - default values, values from
        testconfig.py file and values provided by command line args.
        """
        args_config = self.get_args_config()

        # The order of configs addition in 'config' initialization
        # is relevant - values from each next added config overwrite values of
        # already existing keys.
        config = {**DEFAULT_CONFIG_VALUES, **testconfig.config,
                  **args_config}

        self.convert_to_usable_types(config)

        # Remake dict into namedtuple for more convenient fields aquisition
        return namedtuple('Config', config.keys())(**config)

    def convert_to_usable_types(self, config):
        """
        Converts config values types as parsed from user input into
        types usable by framework implementation
        """
        str2ctx(config)
        str2list(config)
        str2time(config)

    def get_args_config(self):
        """Return config values parsed from command line arguments"""

        # 'groups' positional argument added only if RUNTESTS.py is the
        # execution entry point
        from_runtest = os.path.basename(sys.argv[0]) == 'RUNTESTS.py'
        if from_runtest:
            self.argparser.add_argument('group', nargs='*',
            help='Run only tests from selected groups')

        args = self.argparser.parse_args()

        if from_runtest:
            # remove possible whitespace and empty positional args
            args.group = [g for g in args.group if g and not g.isspace()]

            # test_sequence does not make sense if group is not set
            if args.test_sequence and not args.group:
                self.argparser.error('"--test_sequence" argument needs '
                'to have "group" arg set')

        # make into dict for type consistency
        return {k: v for k, v in vars(args).items() if v is not None}

    def init_argparser(self):
        def ctx_choices(cls):
            return [c.__name__.lower() for c in cls.__subclasses__()]

        parser = argparse.ArgumentParser(
            formatter_class=argparse.RawTextHelpFormatter)
        parser.add_argument('-b', '--build_type',
                            help='run only specified build type',
                            choices=ctx_choices(ctx.Build), nargs='*')
        parser.add_argument('-f', '--fs_type',
                            choices=ctx_choices(ctx.Fs), nargs='*')
        parser.add_argument('-t', '--test-type',
                            help='''run only specified test type where:
                            check = short + medium''',
                            choices=ctx_choices(ctx.TestType), nargs='*')
        parser.add_argument('-m', '--memcheck',
                            choices=['auto', 'force-enable'],
                            help="""run tests with memcheck
        memcheck: auto (default, enable/disable based on test requirements),
        force-enable (enable when test does not require memcheck,
        but obey test's explicit memcheck disable)""",
                            default='auto')
        parser.add_argument('-p', '--pmemcheck',
                            help="""run tests with pmemcheck
        pmemcheck: auto (default, enable/disable based on test requirements),
        force-enable (enable when test does not require pmemcheck,
        but obey test's explicit pmemcheck disable)""",
                            default='auto')
        parser.add_argument('-e', '--helgrind',
                            help="""run tests with helgrind:
        auto (default, enable/disable based on test requirements),
        force-enable (enable when test does not require helgrind,
        but obey test's explicit helgrind disable)""",
                            default='auto')
        parser.add_argument('-d', '--drd',
                            help="""run tests with drd
        drd: auto (default, enable/disable based on test requirements),
        force-enable (enable when test does not require drd, but
        obey test's explicit drd disable)""",
                            default='auto')
        parser.add_argument('-o', '--timeout',
                            help="""set timeout for test execution timeout:
        floating point number with an optional suffix:
        's' for seconds, 'm' for minutes, 'h' for hours or 'd' for days.
        Default value is 3 minutes.""")
        parser.add_argument('-u', '--test-sequence',
                            help="""run only tests from specified test sequence
        e.g.: 0-2,5 will execute TEST0, TEST1, TEST2 and TEST5""",
                            default='')

        return parser
