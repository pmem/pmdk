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
import sys
import os
import shutil
import glob
import subprocess
from datetime import timedelta, datetime
from pathlib import Path
sys.path.insert(0, '../')
from py_parser import *
from testconfig import config

# defaults
# If key is not in dictionary (testconfig.py -> config) then set default value
config["unittest_log_level"] = config.get("unittest_log_level", 2)
config["test_type"] = config.get("test_type", "Check")
config["fs_type"] = config.get("fs_type", "all")
config["build_type"] = config.get("build_type", "Debug")
config["suffix"] = config.get("suffix", "")


#
# KB, MB, GB, TB, PB -- these functions convert the integer to bytes
#
# example:
#   MB(3)  -->  3145728
#   KB(16) -->  16384
#

def KB(n):
    return 2 ** 10 * n


def MB(n):
    return 2 ** 20 * n


def GB(n):
    return 2 ** 30 * n


def TB(n):
    return 2 ** 40 * n


def PB(n):
    return 2 ** 50 * n


#
# str2list -- converts the string to list
#
# example:
#   str2list("0-3,6") -->  [0, 1, 2, 3, 6]
#   str2list("1,3-5") -->  [1, 3, 4, 5]
#
def str2list(arg):
    seq_list = []
    arg = arg.split(",")
    for str in arg:
        if str.find("-"):
            str = str.split("-")
            for x in range(int(str[-2]), int(str[-1])+1):
                seq_list.append(x)
        else:
            seq_list.append(int(str))
    return seq_list


#
# str2time -- converts the string with s, m, h, d suffixes to time format
#
# example:
#   str2time("5s")  -->  "0:00:05"
#   str2time("15m") -->  "0:15:00"
#
def str2time(arg):
    timeout = int(arg[:-1])
    if "d" in arg:
        timeout = timedelta(days=timeout)
    if "m" in arg:
        timeout = timedelta(minutes=timeout)
    if "h" in arg:
        timeout = timedelta(hours=timeout)
    if "s" in arg:
        timeout = timedelta(seconds=timeout)
    return timeout


class Colors:
    """ This class sets the font colour """
    CRED = '\33[91m'
    CGREEN = '\33[92m'
    CEND = '\33[0m'


class Message:
    """ This class checks the value of unittest_log_level
        to print the message. """
    def msg(self, args):
        if config['unittest_log_level'] >= 1:
            print(args)

    def verbose_msg(self, args):
        if config['unittest_log_level'] >= 2:
            print(args)


class Context:
    """ This class sets the context of the variables. """
    def __init__(self):
        self.suffix = config['suffix']
        os.environ['UNITTEST_LOG_LEVEL'] = str(config['unittest_log_level'])

    def create_holey_file(self, size, name):
        """ This method creates a new file with the selected size and name. """
        filepath = "{}/{}".format(self.dir, name)
        if not os.path.exists(self.dir):
            os.makedirs(self.dir)
        with open(filepath, 'w') as f:
            f.seek(size)
            f.write('\0')
        return filepath

    def test_exec(self, cmd):
        """ This method runs the given command depending on the system. """
        if sys.platform == "win32":
            win_cmd = cmd.split()[0]
            win_path = os.path.join(self.exedir, 'tests' )
            win_cmd = win_cmd.replace(".", win_path) + '.exe'
            cmd = "{} {}".format(win_cmd, ' '.join(cmd.split()[1:]))
        else:
            suffix_exe = cmd.split()[0] + self.exesuffix
            cmd = "{} {}".format(suffix_exe, ' '.join(cmd.split()[1:]))
        self.start_time = datetime.now()
        self.ret = subprocess.run(cmd.split(), timeout=str2time(env_timeout).total_seconds())
        self.end_time = datetime.now()


class Build:
    """ This class returns a list of build types based on arguments.
        If class is empty, treats like "all"."""
    def __new__(self, *args):
        if args is ():
            args = (Debug, Nondebug, Static_Debug, Static_Nondebug)
        build = list(args)
        return build


class Fs:
    """ This class returns a list of filesystem types based on arguments.
        If class is empty, treats like "all". """
    def __new__(self, *args):
        if args is ():
            args = (Pmem, Nonpmem)
        Fs = list(args)
        return Fs


class Check:
    """ This class returns a list of test types based on arguments.
        If class is empty, treats like "all". """
    def __new__(self, *args):
        if args is ():
            args = (Short, Medium)
        if All_Types == args:
            args = (Short, Medium, All_Types)
        check = list(args)
        return check


# These empty classes are used to recognize the length of the tests.

class Short(Check):
    pass


class Medium(Check):
    pass


class Large(Check):
    pass


class All_Types(Check):
    pass


class Debug(Build):
    """ This class sets the context for debug build"""
    def setup_context(ctx):
        ctx.exesuffix = ''
        if sys.platform == "win32":
            ctx.exedir = "..\..\\x64\Debug"
            os.environ['PMDK_LIB_PATH_DEBUG'] = os.path.join(ctx.exedir, 'libs')
        else:
            os.environ['LD_LIBRARY_PATH'] = '../../debug'


class Nondebug(Build):
    """ This class sets the context for nondebug build"""
    def setup_context(ctx):
        ctx.exesuffix = ''
        if sys.platform == "win32":
            ctx.exedir = '../../x64/Release'
            os.environ['PMDK_LIB_PATH_NONDEBUG'] = os.path.join(ctx.exedir, 'libs')
        else:
            os.environ['LD_LIBRARY_PATH'] = '../../nondebug'


class Static_Debug(Build):
    """ This class sets the context for static_debug build"""
    def setup_context(ctx):
        ctx.exesuffix = '.static-debug'
        os.environ['LD_LIBRARY_PATH'] = '../../debug'


class Static_Nondebug(Build):
    """ This class sets the context for static_nondebug build"""
    def setup_context(ctx):
        ctx.exesuffix = '.static-nondebug'
        os.environ['LD_LIBRARY_PATH'] = '../../nondebug'


class Pmem(Fs):
    """ This class sets the context for pmem filesystem"""
    def setup_context(ctx):
        ctx.dir = "{}//test_{}{}{}".format(config['pmem_fs_dir'], \
		os.path.basename(os.getcwd()), \
		ctx.unittest_num, \
		ctx.suffix)


class Nonpmem(Fs):
    """ This class sets the context for nonpmem filesystem"""
    def setup_context(ctx):
        ctx.dir = "{}//test_{}{}{}".format(config['non_pmem_fs_dir'], \
		os.path.basename(os.getcwd()), \
		ctx.unittest_num, \
		ctx.suffix)


class Executor:
    """ This class is responsible for managing the test,
        e.g. creating and deleting files, checking logs, running the test. """
    def match(self, ctx, test_type):
        """ Matches log files. """
        perl = ""
        if ctx.ret.returncode != 0:
            self.fail()
        else:
            if sys.platform == "win32":
                perl = "perl"
            files = glob.glob("{}/*[a-z]{}.log.match".format( \
			os.getcwd(), ctx.unittest_num), recursive=True)
            for f in files:
                cmd = Path("{} ../match {}".format(perl, f))
                ret = os.system(str(cmd))
                if ret != 0:
                    self.fail()
                    sys.exit(1)
            self.test_passed(ctx, test_type)

    def fail(self):
        """ Prints fail message. """
        Message().msg("{}FAILED {}".format(Colors.CRED, Colors.CEND))

    def clean(self, dirname):
        """ Removes directory, even if it is not empty. """
        shutil.rmtree("{}".format(dirname), ignore_errors=True)

    def test_passed(self, ctx, test_type):
        """ Pass the test if the result is lower than timeout.
            Otherwise, depending on the "keep_going" variable, continue running tests. """
        delta = ctx.end_time - ctx.start_time
        if Large in test_type:
            if delta > str2time(env_timeout):
                Message().msg("Skipping: {} {}timed out{}".format( \
				ctx.unittest_name, Colors.CRED, Colors.CEND))
                try:
                    config['keep_going'] == 'y'
                except:
                    subprocess.check_call(["pkill", "-f", "RUNTESTS.py"])
                    sys.exit()
        if delta.total_seconds() < 61:
            sec_test = float(delta.total_seconds())
            delta = "%06.3f" % sec_test
        if config.get('tm') == 1:
            tm = "\t\t\t[{}] s".format(delta)
        else:
            tm = ''
        Message().msg("{}: {}PASS {} {}".format( \
		ctx.unittest_name, Colors.CGREEN, Colors.CEND, tm))

    def run(self, test, *args):
        """ Run the test with required variables. """
        testnum = test.__class__.__name__.replace("launch", "")
        ctx = Context()
        os.environ['UNITTEST_NAME'] = ctx.unittest_name = "{}/TEST{}".format( \
		os.path.basename(os.getcwd()), testnum)
        ctx.unittest_num = testnum
        os.environ['UNITTEST_NUM'] = testnum
        fs_type, build_type, test_type = [], [], []

        def check_global():
            """ Check global variables and return False
                if they do not comply with the test requirements  """
            if env_testfile != "all":
                if env_testfile != "TEST{}".format(testnum):
                    return False

            if env_testseq != '':
                if int(testnum) not in str2list(env_testseq):
                    return False

            if env_build != "all":
                _build = getattr(sys.modules[__name__], env_build)
                if _build in build_type:
                    build_type.clear()
                    build_type.append(_build)
                else:
                    return False

            if env_fs != "all":
                _fs = getattr(sys.modules[__name__], env_fs)
                if _fs in fs_type:
                    fs_type.clear()
                    fs_type.append(_fs)
                else:
                    return False

        # create list of different types, based on the inheritance
        for group_type in args:
            for subclass in group_type:
                if issubclass(subclass, Build):
                    build_type.append(subclass)
                if issubclass(subclass, Fs):
                    fs_type.append(subclass)
                if issubclass(subclass, Check):
                    test_type.append(subclass)

        # if the list is empty, treat it as if it contains everything
        if fs_type == []:
            fs_type = Fs()
        if build_type == []:
            build_type = Build()

        if check_global() is False:
            return

        for f in fs_type:
            f.setup_context(ctx)
            for b in build_type:
                b.setup_context(ctx)
                try:
                    print("{}: SETUP ".format(ctx.unittest_name) + \
					str(Path("({}/{})".format(f.__name__, b.__name__))))
                except:
                    print("{}: SETUP ".format(ctx.unittest_name) + str(Path("({})".format(f.__name__))))
                test.run(ctx)
                self.match(ctx, test_type)
                self.clean("{}".format(ctx.dir))
