# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation
#
"""Parser for user provided test configuration"""

import argparse
import os
import string
import sys
from datetime import timedelta

import builds
import context as ctx
import granularity
import futils
import test_types
import valgrind as vg

try:
    import testconfig
except ImportError:
    sys.exit('Please add valid testconfig.py file - see testconfig.py.example')


class _ConfigFromDict:
    """
    Class fields are created from provided dictionary. Used for creating
    a final config object.
    """
    def __init__(self, dict_):
        for k, v in dict_.items():
            setattr(self, k, v)

    # special method triggered if class attribute was not found
    # https://docs.python.org/3.5/reference/datamodel.html#object.__getattr__
    def __getattr__(self, name):
        if name == 'page_fs_dir':
            raise futils.Skip('Configuration field "{}" not found. '
                              'No page granularity test directory '
                              'provided'.format(name))

        if name == 'cacheline_fs_dir':
            raise futils.Skip('Configuration field "{}" not found. '
                              'No cache line granularity test '
                              'directory provided'.format(name))

        if name == 'byte_fs_dir':
            raise futils.Skip('Configuration field "{}" not found. '
                              'No byte granularity test directory '
                              'provided'.format(name))

        raise AttributeError('Provided test configuration may be '
                             'invalid. No "{}" field found in '
                             'configuration.'
                             .format(name))


def _str2list(config):
    """
    Convert the string with test sequence to equivalent list.
    example:
    _str2list("0-3,6") -->  [0, 1, 2, 3, 6]
    _str2list("1,3-5") -->  [1, 3, 4, 5]
    """
    arg = config['test_sequence']
    if not arg:
        # test_sequence not set, do nothing
        return

    seq = []
    try:
        for number in arg.split(','):
            if '-' in number:
                number = number.split('-')
                begin = int(number[0])
                end = int(number[1])
                step = 1 if begin < end else -1
                seq.extend(range(begin, end + step, step))
            else:
                seq.append(int(number))

    except (ValueError, IndexError):
        print('Provided test sequence "{}" is invalid'.format(arg))
        raise

    config['test_sequence'] = seq


def _str2time(config):
    """
    Convert the string with s, m, h, d suffixes to time format

    example:
    _str2time("5s")  -->  "0:00:05"
    _str2time("15m") -->  "0:15:00"
    """
    string_ = config['timeout']
    try:
        timeout = int(string_[:-1])
    except ValueError as e:
        raise ValueError("invalid timeout argument: {}".format(string_)) from e
    else:
        if "d" in string_:
            timeout = timedelta(days=timeout)
        elif "m" in string_:
            timeout = timedelta(minutes=timeout)
        elif "h" in string_:
            timeout = timedelta(hours=timeout)
        elif "s" in string_:
            timeout = timedelta(seconds=timeout)

        config['timeout'] = timeout.total_seconds()


def _str2ctx(config):
    """Convert context classes from strings to actual classes"""
    def class_from_string(name, base):
        if name == 'all':
            return base.__subclasses__()

        try:
            return next(b for b in base.__subclasses__()
                        if str(b) == name.lower())
        except StopIteration:
            print('Invalid config value: "{}".'.format(name))
            raise

    def convert_internal(key, base):
        if not isinstance(config[key], list):
            config[key] = ctx.expand(class_from_string(config[key], base))
        else:
            classes = [class_from_string(cl, base) for cl in config[key]]
            config[key] = ctx.expand(*classes)

    convert_internal('build', builds.Build)
    convert_internal('test_type', test_types._TestType)
    convert_internal('granularity', granularity.Granularity)

    if config['force_enable'] is not None:
        config['force_enable'] = next(
            t for t in vg.TOOLS
            if t.name.lower() == config['force_enable'])


class Configurator():
    """Parser for user test configuration.

    Configuration is generated from two sources: testconfig.py
    file and main script (RUNTESTS.py or TESTS.py files)
    command line arguments.
    Since these sources cannot change during script execution,
    the configurator class can be initialized multiple times
    throughout the implementation and always returns the same
    configuration result.

    Values from testconfig.py are overridden by respective
    values from command line arguments - provided the latter occur.

    Attributes:
        config: final test configuration meant to be used by
            the user
    """

    def __init__(self):
        self.config = self.parse_config()

    def parse_config(self):
        """
        Parse and return test execution config object. Final config is
        composed from 2 config values - values from testconfig.py file
        and values provided by command line args.
        """
        self.argparser = self._init_argparser()
        try:
            args_config = self._get_args_config()

            # The order of configs addition in 'config' initialization
            # is relevant - values from each next added config overwrite
            # values of already existing keys.
            config = {**testconfig.config, **args_config}

            self._convert_to_usable_types(config)

            # Remake dict into class object for convenient fields acquisition
            config = _ConfigFromDict(config)

            # device_dax_path may be either a single string with path
            # or a sequence of paths
            config.device_dax_path = futils.to_list(config.device_dax_path,
                                                    str)

            return config

        except KeyError as e:
            print("No config field '{}' found. "
                  "testconfig.py file may be invalid.".format(e.args[0]))
            raise

    def _convert_to_usable_types(self, config):
        """
        Converts config values types as parsed from user input into
        types usable by framework implementation
        """
        _str2ctx(config)
        _str2list(config)
        _str2time(config)

    def _get_args_config(self):
        """Return config values parsed from command line arguments"""

        # 'group' positional argument added only if RUNTESTS.py is the
        # execution entry point
        from_runtest = os.path.basename(sys.argv[0]) == 'RUNTESTS.py'
        if from_runtest:
            self.argparser.add_argument('group', nargs='*',
                                        help='Run only tests '
                                        'from selected groups')

        # remove possible whitespace and empty args
        sys.argv = [arg for arg in sys.argv if arg and not arg.isspace()]

        args = self.argparser.parse_args()

        if from_runtest:
            # test_sequence does not make sense if group is not set
            if args.test_sequence and not args.group:
                self.argparser.error('"--test_sequence" argument needs '
                                     'to have "group" arg set')

            # remove possible path characters added by shell hint
            args.group = [g.strip(string.punctuation) for g in args.group]

        # make into dict for type consistency
        return {k: v for k, v in vars(args).items() if v is not None}

    def _init_argparser(self):
        def ctx_choices(cls):
            return [str(c) for c in cls.__subclasses__()]

        parser = argparse.ArgumentParser()
        parser.add_argument('--fs_dir_force_pmem', type=int,
                            help='set PMEM_IS_PMEM_FORCE for tests run on'
                            ' pmem fs')
        parser.add_argument('-l', '--unittest_log_level', type=int,
                            help='set log level. 0 - silent, 1 - normal, '
                            '2 - verbose')
        parser.add_argument('--keep_going', type=bool,
                            help='continue execution despite test fails')
        parser.add_argument('-b', dest='build',
                            help='run only specified build type',
                            choices=ctx_choices(builds.Build), nargs='*')
        parser.add_argument('-g', dest='granularity',
                            choices=ctx_choices(granularity.Granularity),
                            nargs='*', help='run tests on a filesystem'
                            ' with specified granularity types.')
        parser.add_argument('-t', dest='test_type',
                            help='run only specified test type where '
                            'check = short + medium',
                            choices=ctx_choices(test_types._TestType),
                            nargs='*')
        parser.add_argument('-o', dest='timeout',
                            help="set timeout for test execution timeout: "
                            "integer with an optional suffix:''s' for seconds,"
                            " 'm' for minutes, 'h' for hours or 'd' for days.")
        parser.add_argument('-u', dest='test_sequence',
                            help='run only tests from specified test sequence '
                            'e.g.: 0-2,5 will execute TEST0, '
                            'TEST1, TEST2 and TEST5',
                            default='')
        parser.add_argument('--list-testcases', dest='list_testcases',
                            action='store_const', const=True,
                            help='List testcases only')
        parser.add_argument('--fail-on-skip', dest='fail_on_skip',
                            action='store_const', const=True,
                            help='Skipping tests also fail')

        tracers = parser.add_mutually_exclusive_group()
        tracers.add_argument('--tracer', dest='tracer', help='run C binary '
                             'with provided tracer command. With this option '
                             'stdout and stderr are not redirected, enabling '
                             'interactive sessions.',
                             default='')
        tracers.add_argument('--gdb', dest='tracer', action='store_const',
                             const='gdb --args', help='run gdb as a tracer')
        tracers.add_argument('--cgdb', dest='tracer', action='store_const',
                             const='cgdb --args', help='run cgdb as a tracer')

        fe_choices = [str(t) for t in vg.TOOLS]
        parser.add_argument('--force-enable', choices=fe_choices, default=None)

        return parser
