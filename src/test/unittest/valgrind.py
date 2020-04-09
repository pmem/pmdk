# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#
"""Valgrind handling tools"""

import sys
import re
import subprocess as sp
from enum import Enum, unique
from os import path

import context as ctx
import futils


DISABLE = -1
ENABLE = 1
AUTO = 0

_IGNORED = (
    "WARNING: Serious error when reading debug info",
    "When reading debug info from ",
    "Ignoring non-Dwarf2/3/4 block in .debug_info",
    "Last block truncated in .debug_info; ignoring",
    "parse_CU_Header: is neither DWARF2 nor DWARF3 nor DWARF4",
    "brk segment overflow",
    "see section Limitations in user manual",
    "Warning: set address range perms: large range",
    "further instances of this message will not be shown",
    "get_Form_contents: DW_FORM_GNU_strp_alt used, but no alternate .debug_str"
)


@unique
class _Tool(Enum):
    """
    Enumeration of valid Valgrind tool options.

    NONE is a virtual tool option denoting that valgrind
    is not used for test execution.
    """
    MEMCHECK = 1
    PMEMCHECK = 2
    HELGRIND = 3
    DRD = 4
    NONE = 5

    def __str__(self):
        return self.name.lower()

    def __bool__(self):
        return self != self.NONE


TOOLS = tuple(t for t in _Tool if t != _Tool.NONE)

MEMCHECK = _Tool.MEMCHECK
PMEMCHECK = _Tool.PMEMCHECK
HELGRIND = _Tool.HELGRIND
DRD = _Tool.DRD
NONE = _Tool.NONE


class Valgrind:
    """Valgrind management context element class

    Attributes:
        tool (_Tool): selected valgrind tool
        tool_name (str): tool name as string
        cwd (str): path to the test cwd
            (i. e. the directory in which test .py file resides)
        log_file (str): path to valgrind output log file
        valgrind_exe (str): path to valgrind command
        opts (list): list of valgrind command line options

    """

    def __init__(self, tool, cwd, testnum):
        if sys.platform == 'win32':
            raise NotImplementedError(
                'Valgrind class should not be used on Windows')

        self.tool = NONE if tool is None else tool
        self.tool_name = self.tool.name.lower()
        self.cwd = cwd

        log_file_name = '{}{}.log'.format(self.tool.name.lower(), testnum)
        self.log_file = path.join(cwd, log_file_name)

        if self.tool == NONE:
            self.valgrind_exe = None
        else:
            self.valgrind_exe = self._get_valgrind_exe()

        if self.valgrind_exe is None:
            return

        self.verify()

        self.opts = []

        self.add_suppression('ld.supp')

        if 'freebsd' in sys.platform:
            self.add_suppression('freebsd.supp')

        if tool == MEMCHECK:
            self.add_suppression('memcheck-libunwind.supp')
            self.add_suppression('memcheck-ndctl.supp')
            self.add_suppression('memcheck-dlopen.supp')

        # Before Skylake, Intel CPUs did not have clflushopt instruction, so
        # pmem_flush and pmem_persist both translated to clflush.
        # This means that missing pmem_drain after pmem_flush could only be
        # detected on Skylake+ CPUs.
        # This option tells pmemcheck to expect fence (sfence or
        # VALGRIND_PMC_DO_FENCE client request, used by pmem_drain) after
        # clflush and makes pmemcheck output the same on pre-Skylake and
        # post-Skylake CPUs.
        elif tool == PMEMCHECK:
            self.add_opt('--expect-fence-after-clflush=yes')

        elif tool == HELGRIND:
            self.add_suppression('helgrind-log.supp')

        elif tool == DRD:
            self.add_suppression('drd-log.supp')

    def __str__(self):
        return self.tool.name.lower()

    def __bool__(self):
        return self.tool != NONE

    @classmethod
    def filter(cls, config, msg, tc):
        """
        Acquire Valgrind tool for the test to be run with.
        Takes into account configuration 'force-enable' options,
        and Valgrind tools enabled or disabled by test requirements.

        Args:
            config: configuration as returned by Configurator class
            msg (Message): level based logger class instance
            tc (BaseTest): test case, from which the Valgrind
                requirements are obtained

        Returns:
            list of initialized Valgrind tool classes with which the test
            should be run

        """
        vg_tool, kwargs = ctx.get_requirement(tc, 'enabled_valgrind', NONE)
        disabled, _ = ctx.get_requirement(tc, 'disabled_valgrind', ())

        if config.force_enable:
            if vg_tool and vg_tool != config.force_enable:
                raise futils.Skip(
                    "test enables the '{}' Valgrind tool while "
                    "execution configuration forces '{}'"
                    .format(vg_tool, config.force_enable))

            elif config.force_enable in disabled:
                raise futils.Skip(
                      "forced Valgrind tool '{}' is disabled by test"
                      .format(config.force_enable))

            else:
                vg_tool = config.force_enable

        return [cls(vg_tool, tc.cwd, tc.testnum, **kwargs), ]

    @property
    def cmd(self):
        """
        Return Valgrind command with specified arguments
        as subprocess compliant list.
        """
        if self.tool == NONE:
            return []

        cmd = [self.valgrind_exe, '--tool={}'.format(self.tool_name),
               '--log-file={}'.format(self.log_file)] + self.opts
        return cmd

    def setup(self, memcheck_check_leaks=True, **kwargs):
        if self.tool == MEMCHECK and memcheck_check_leaks:
            self.add_opt('--leak-check=full')

    def check(self, **kwargs):
        self.validate_log()

    def _get_valgrind_exe(self):
        """
        On some systems "valgrind" is a shell script that calls the actual
        executable "valgrind.bin".
        The wrapper script does not work well with LD_PRELOAD so we want
        to call Valgrind directly.
        """
        try:
            out = sp.check_output('which valgrind', shell=True,
                                  universal_newlines=True)
        except sp.CalledProcessError:
            raise futils.Skip('Valgrind not found')

        valgrind_bin = path.join(path.dirname(out), 'valgrind.bin')
        if path.isfile(valgrind_bin):
            return valgrind_bin
        return 'valgrind'

    def add_opt(self, opt):
        """Add option to Valgrind command"""
        self.opts.append(opt)

    def _get_version(self):
        """
        Get Valgrind version represented as integer with patch version ignored
        """
        out = sp.check_output('{} --version'.format(self.valgrind_exe),
                              shell=True, universal_newlines=True)
        version = out.split('valgrind-')[1]
        version_as_int = int(version.rsplit('.', 1)[0].replace('.', ''))
        return version_as_int

    def add_suppression(self, f):
        """
        Add suppression file. Provided file path is
        relative to tests root directory (pmdk/src/test)
        """
        self.opts.append('--suppressions={}'
                         .format(path.join(futils.ROOTDIR, f)))

    def validate_log(self):
        """
        Check Valgrind test result based on Valgrind log file.
        Return True if passed, False otherwise
        """
        if self.tool == NONE or sys.platform == 'win32':
            return True

        no_ignored = []
        # remove ignored warnings from log file
        with open(self.log_file, 'r+') as f:
            no_ignored = [ln for ln in f if not any(w in ln for w in _IGNORED)]
            f.seek(0)
            f.writelines(no_ignored)
            f.truncate()

        if path.isfile(self.log_file + '.match'):
            # if there is a Valgrind log match file, do nothing - log file
            # will be checked by 'match' tool
            return

        non_zero_errors = 'ERROR SUMMARY: [^0]'
        errors_found = any(re.search(non_zero_errors, ln) for ln in no_ignored)
        if any('Bad pmempool' in ln for ln in no_ignored) or errors_found:
            raise futils.Fail('Valgrind log validation failed')

    def verify(self):
        """
        Check that Valgrind is viable to be used.
        """
        if self.valgrind_exe is None:
            raise futils.Skip('Valgrind not found')

        # verify tool
        cmd = '{} --tool={} --help'.format(self.valgrind_exe, self.tool_name)
        try:
            sp.check_output(cmd, shell=True, stderr=sp.STDOUT)
        except sp.CalledProcessError:
            raise futils.Skip("Valgrind tool '{}' was not found"
                              .format(self.tool_name))


def require_valgrind_enabled(valgrind):
    """
    Enable valgrind tool for given test.

    Used as a test class (tc) decorator.

    Args:
        valgrind (str): valgrind tool

    """

    def wrapped(tc):
        if sys.platform == 'win32':
            # do not run valgrind tests on windows
            tc.enabled = False
            return tc

        tool = _require_valgrind_common(valgrind)
        ctx.add_requirement(tc, 'enabled_valgrind', tool)

        return tc

    return wrapped


def require_valgrind_disabled(*valgrind):
    """
    Require that the test is not executed with selected valgrind tools.

    Used as a test class (tc) decorator.

    Args:
        *valgrind (*str): variable length arguments list of valgrind
            tools meant to be disabled

    """

    def wrapped(tc):
        disabled_tools = [_require_valgrind_common(v) for v in valgrind]
        ctx.add_requirement(tc, 'disabled_valgrind', disabled_tools)

        return tc

    return wrapped


def _require_valgrind_common(v):
    """
    Validate provided valgrind tool as string
    and translate it to _Tool enum type.

    Args:
        v (str): valgrind tool as string

    Returns:
        valgrind tool as _Tool enum type

    """
    valid_tool_names = [str(t) for t in TOOLS]
    if v not in valid_tool_names:
        sys.exit('used name {} not in valid valgrind tool names which are: {}'
                 .format(v, valid_tool_names))

    str_to_tool = next(t for t in TOOLS if v == str(t))
    return str_to_tool
