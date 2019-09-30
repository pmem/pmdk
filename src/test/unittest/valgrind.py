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
"""Valgrind handling tools"""

import sys
import re
import subprocess as sp
from enum import Enum, unique
from os import path

import futils
from utils import VMMALLOC


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
    MEMCHECK = 1
    PMEMCHECK = 2
    HELGRIND = 3
    DRD = 4
    NONE = 5


TOOLS = tuple(t for t in _Tool if t != _Tool.NONE)

MEMCHECK = _Tool.MEMCHECK
PMEMCHECK = _Tool.PMEMCHECK
HELGRIND = _Tool.HELGRIND
DRD = _Tool.DRD
NONE = _Tool.NONE


def enabled_tool(test):
    """Get Valgrind tool enabled by test"""
    enabled = [t for t in TOOLS if getattr(test, t.name.lower()) == ENABLE]
    if len(enabled) > 1:
        raise ValueError('test "{}" enables more than one Valgrind tool'
                         .format(test))
    elif len(enabled) == 1:
        return enabled[0]
    else:
        return None


def disabled_tools(test):
    """Get Valgrind tools disabled by test"""
    disabled = [t for t in TOOLS if getattr(test, t.name.lower()) == DISABLE]
    return disabled


class Valgrind:
    """Valgrind management"""

    def __init__(self, tool, cwd, testnum, memcheck_check_leaks=True):
        if sys.platform == 'win32':
            raise NotImplementedError(
                'Valgrind class should not be used on Windows')

        self.tool = NONE if tool is None else tool
        self.tool_name = self.tool.name.lower()
        self.cwd = cwd

        log_file_name = '{}{}.log'.format(self.tool.name.lower(), testnum)
        self.log_file = path.join(cwd, log_file_name)

        if tool is None:
            self.valgrind_exe = None
        else:
            self.valgrind_exe = self._get_valgrind_exe()

        if self.valgrind_exe is None:
            return

        self.opts = ''
        self.memcheck_check_leaks = memcheck_check_leaks

        self.add_suppression('ld.supp')

        if 'freebsd' in sys.platform:
            self.add_suppression('freebsd.supp')

        if tool == MEMCHECK:
            self.add_suppression('memcheck-libunwind.supp')
            self.add_suppression('memcheck-ndctl.supp')
            self.add_suppression('memcheck-dlopen.supp')
            if memcheck_check_leaks:
                self.add_opt('--leak-check=full')

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

    @property
    def cmd(self):
        """Get Valgrind command with specified arguments"""
        if self.tool == NONE:
            return ''
        return '{} --tool={} --log-file={} {} '.format(
            self.valgrind_exe, self.tool_name, self.log_file, self.opts)

    def _get_valgrind_exe(self):
        """
        On some systems "valgrind" is a shell script that calls the actual
        executable "valgrind.bin".
        The wrapper script does not work well with LD_PRELOAD so we want
        to call Valgrind directly
        """
        try:
            out = sp.check_output('which valgrind', shell=True,
                                  universal_newlines=True)
        except sp.CalledProcessError:
            return None

        valgrind_bin = path.join(path.dirname(out), 'valgrind.bin')
        if path.isfile(valgrind_bin):
            return valgrind_bin
        return 'valgrind'

    def add_opt(self, opt, tool=None):
        """Add option to Valgrind command"""
        if tool is None or tool == self.tool:
            self.opts = '{} {}'.format(self.opts, opt)

    def _get_version(self):
        """
        Get Valgrind version represented as integer with patch version ignored
        """
        out = sp.check_output('{} --version'.format(self.valgrind_exe),
                              shell=True, universal_newlines=True)
        version = out.split('valgrind-')[1]
        version_as_int = int(version.rsplit('.', 1)[0].replace('.', ''))
        return version_as_int

    def handle_ld_preload(self, ld_preload):
        """Handle Valgrind setup for given LD_PRELOAD value"""
        if self._get_version() >= 312 and \
           path.basename(ld_preload) == VMMALLOC:
            self.add_opt('--soname-synonyms=somalloc=nouserintercepts')

    def add_suppression(self, f):
        """
        Add suppression file. Provided file path is
        relative to tests root directory (pmdk/src/test)
        """
        self.opts = '{} --suppressions={}'.format(
            self.opts, path.join(futils.ROOTDIR, f))

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
            no_ignored = [l for l in f if not any(w in l for w in _IGNORED)]
            f.seek(0)
            f.writelines(no_ignored)
            f.truncate()

        if path.isfile(self.log_file + '.match'):
            # if there is a Valgrind log match file, do nothing - log file
            # will be checked by 'match' tool
            return True

        non_zero_errors = 'ERROR SUMMARY: [^0]'
        errors_found = any(re.search(non_zero_errors, l) for l in no_ignored)
        if any('Bad pmempool' in l for l in no_ignored) or errors_found:
            return False

        return True

    def verify(self):
        """
        Checks that Valgrind can be used.
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
