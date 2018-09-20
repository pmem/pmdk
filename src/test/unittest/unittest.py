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
config["test_type"] = config.get("test_type", "check")
config["fs_type"] = config.get("fs_type", "all")
config["build_type"] = config.get("build_type", "debug")
config["suffix"] = config.get("suffix", "😘⠝⠧⠍⠇ɗPMDKӜ⥺🙋")


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
    for i in arg:
        if i.find("-"):
            i = i.split("-")
            for x in range(int(i[-2]), int(i[-1])+1):
                seq_list.append(x)
        else:
            seq_list.append(int(i))
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


class colors:
    """ This class sets the font colour """
    CRED = '\33[91m'
    CGREEN = '\33[92m'
    CEND = '\33[0m'


class message:
    """ This class checks the value of unittest_log_level
        to print the message. """
    def msg(self, args):
        if config['unittest_log_level'] >= 1:
            print(args)

    def verbose_msg(self, args):
        if config['unittest_log_level'] >= 2:
            print(args)


class context:
    """ This class sets the context of the variables. """
    def __init__(self):
        self.suffix = config['suffix']
        os.environ['UNITTEST_LOG_LEVEL'] = str(config['unittest_log_level'])

    def create_holey_file(self, size, name):
        """ This method creates a new file with the selected size and name. """
        filepath = Path(F'{self.dir}/{name}')
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
            win_cmd = win_cmd.replace(".", self.exedir) + '.exe'
            cmd = F"{win_cmd} {' '.join(cmd.split()[1:])}"
        else:
            suffix_exe = cmd.split()[0] + self.exesuffix
            cmd = F" {suffix_exe} {' '.join(cmd.split()[1:])}"
        self.ret = os.system(str(Path(cmd)))


class build:
    """ This class returns a list of build types based on arguments.
        If class is empty, treats like "all"."""
    def __new__(self, *args):
        if args is ():
            args = (debug, nondebug, static_debug, static_nondebug)
        build = list(args)
        return build


class fs:
    """ This class returns a list of filesystem types based on arguments.
        If class is empty, treats like "all". """
    def __new__(self, *args):
        if args is ():
            args = (pmem, nonpmem)
        fs = list(args)
        return fs


class check:
    """ This class returns a list of test types based on arguments.
        If class is empty, treats like "all". """
    def __new__(self, *args):
        if args is ():
            args = (short, medium)
        if all_types == args:
            args = (short, medium, all_types)
        check = list(args)
        return check


# These empty classes are used to recognize the length of the tests.

class short(check):
    pass


class medium(check):
    pass


class large(check):
    pass


class all_types(check):
    pass


class debug(build):
    """ This class sets the context for debug build"""
    def setup_context(ctx):
        ctx.exesuffix = ''
        os.environ['LD_LIBRARY_PATH'] = '../../debug'
        if sys.platform == "win32":
            ctx.exedir = "../../x64/debug"


class nondebug(build):
    """ This class sets the context for nondebug build"""
    def setup_context(ctx):
        ctx.exesuffix = ''
        os.environ['LD_LIBRARY_PATH'] = '../../nondebug'
        if sys.platform == "win32":
            ctx.exedir = '../../x64/release'


class static_debug(build):
    """ This class sets the context for static_debug build"""
    def setup_context(ctx):
        ctx.exesuffix = '.static-debug'
        os.environ['LD_LIBRARY_PATH'] = '../../debug'


class static_nondebug(build):
    """ This class sets the context for static_nondebug build"""
    def setup_context(ctx):
        ctx.exesuffix = '.static-nondebug'
        os.environ['LD_LIBRARY_PATH'] = '../../nondebug'


class pmem(fs):
    """ This class sets the context for pmem filesystem"""
    def setup_context(ctx):
        ctx.dir = Path(F"{config['pmem_fs_dir']}//test_{os.path.basename(os.getcwd())}{ctx.unittest_num}{ctx.suffix}")


class nonpmem(fs):
    """ This class sets the context for nonpmem filesystem"""
    def setup_context(ctx):
        ctx.dir = Path(F"{config['non_pmem_fs_dir']}//test_{os.path.basename(os.getcwd())}{ctx.unittest_num}{ctx.suffix}")


class executor:
    """ This class is responsible for managing the test,
        e.g. creating and deleting files, checking logs, counting time, running the test. """
    def match(self, ctx, test_type):
        """ Matches log files. """
        perl = ""
        if ctx.ret != 0:
            self.fail()
        else:
            if sys.platform == "win32":
                perl = "perl"
            files = glob.glob(F'{os.getcwd()}/*[a-z]{ctx.unittest_num}.log.match', recursive=True)
            for f in files:
                cmd = Path(F'{perl} ../match {f}')
                ret = os.system(str(cmd))
                if ret != 0:
                    self.fail()
                    sys.exit(1)
            self.test_passed(ctx, test_type)

    def fail(self):
        """ Prints fail message. """
        message().msg(F'{colors.CRED}FAILED {colors.CEND}')

    def clean(self, dirname):
        """ Removes directory, even if it is not empty. """
        shutil.rmtree(F'{dirname}', ignore_errors=True)

    def timed_test(self):
        """ Starts counting the time. """
        global start_time
        self.start_time = datetime.now()

    def test_passed(self, ctx, test_type):
        """ Pass the test if the result is lower than timeout.
            Otherwise, depending on the "keep_going" variable, continue running tests. """
        end_time = datetime.now()
        delta = end_time - self.start_time
        if large in test_type:
            if delta > str2time(env_timeout):
                message().msg(F"Skipping: {ctx.unittest_name} {colors.CRED}timed out{colors.CEND}")
                try:
                    config['keep_going']
                except:
                    subprocess.check_call(["pkill", "-f", "RUNTESTS.py"])
                    sys.exit()

        if delta.total_seconds() < 61:
            sec_test = float(delta.total_seconds())
            delta = "%06.3f" % sec_test
        try:
            config['tm']
            tm = F"\t\t\t[{delta}] s"
        except:
            tm = ''
        message().msg(F'{ctx.unittest_name}: {colors.CGREEN}PASS {colors.CEND} {tm}')

    def run(self, test, *args):
        """ Run the test with required variables. """
        testnum = test.__class__.__name__.replace("launch", "")
        ctx = context()
        os.environ['UNITTEST_NAME'] = ctx.unittest_name = F'{os.path.basename(os.getcwd())}/TEST{testnum}'
        ctx.unittest_num = testnum
        os.environ['UNITTEST_NUM'] = testnum
        fs_type, build_type, valgrind_type, test_type = [], [], [], []

        def check_global():
            """ Check global variables and return False
                if they do not comply with the test requirements  """
            if env_testfile != "all":
                if env_testfile != F"TEST{testnum}":
                    return False

            if env_testseq != '':
                if int(testnum) not in str2list(env_testseq):
                    return False

            if env_build != "all":
                if eval(env_build) in build_type:
                    build_type.clear()
                    build_type.append(eval(env_build))
                else:
                    return False

            if env_fs != "all":
                if eval(env_fs) in fs_type:
                    fs_type.clear()
                    fs_type.append(eval(env_fs))
                else:
                    return False

            if env_trace != "check":
                if eval(env_trace) in valgrind_type:
                    valgrind_type.clear()
                    valgrind_type.append(eval(env_trace))
                else:
                    return False

        # create list of different types, based on the inheritence
        for arg in args:
            for i in arg:
                if issubclass(i, build):
                    build_type.append(i)
                if issubclass(i, fs):
                    fs_type.append(i)
                if issubclass(i, check):
                    test_type.append(i)

        # If the list is empty, treat it as if it contains everything
        if fs_type == []:
            fs_type = fs()
        if build_type == []:
            build_type = build()

        if check_global() is False:
            return

        for f in fs_type:
            f.setup_context(ctx)
            for b in build_type:
                b.setup_context(ctx)
                try:
                    print(F'{ctx.unittest_name}: SETUP ' + str(Path(F'({f.__name__}/{b.__name__})')))
                except:
                    print(F'{ctx.unittest_name}: SETUP ' + str(Path(F'({f.__name__})')))
                self.timed_test()
                test.run(ctx)
                self.match(ctx, test_type)
                self.clean(F'{ctx.dir}')
