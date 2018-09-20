import argparse
import sys
import glob
from pathlib import Path

parser = argparse.ArgumentParser(formatter_class=argparse.RawTextHelpFormatter)

# removes all empty strings from command-line arguments passed to the script
sys.argv = list(filter(lambda el: el != "", sys.argv))

parser.add_argument('-b', metavar="build_type",
                    help="""run only specified build type
build-type: debug, nondebug, static-debug, static-nondebug,
all (default)""",
                    default='all')
parser.add_argument('-f', metavar="fs_type",
                    help="""run tests only on specified file systems
fs-type: pmem, non-pmem, any, none, all (default)""",
                    default='all')
parser.add_argument("-t", metavar="test-type",
                    help="""run only specified test type
test-type: check (default), short, medium, long, all
where: check = short + medium; all = short + medium + long""",
                    default='check')
parser.add_argument("-s", metavar="test-file",
                    help="""run only specified test file
test-file: all (default), TEST0, TEST1, ...""",
                    default='all')
parser.add_argument("-m", metavar="memcheck",
                    help="""run tests with memcheck
memcheck: auto (default, enable/disable based on test requirements),
force-enable (enable when test does not require memcheck,
but obey test's explicit memcheck disable)""",
                    default='auto')
parser.add_argument("-p", metavar="pmemcheck",
                    help="""run tests with pmemcheck
pmemcheck: auto (default, enable/disable based on test requirements),
force-enable (enable when test does not require pmemcheck,
but obey test's explicit pmemcheck disable)""",
                    default='auto')
parser.add_argument("-e", metavar="helgrind",
                    help="""run tests with helgrind helgrind:
auto (default, enable/disable based on test requirements),
force-enable (enable when test does not require helgrind,
but obey test's explicit helgrind disable)""",
                    default='auto')
parser.add_argument("-d",
                    metavar="drd",
                    help="""run tests with drd
drd: auto (default, enable/disable based on test requirements),
force-enable (enable when test does not require drd, but
obey test's explicit drd disable)""",
                    default='auto')
parser.add_argument("-o",
                    metavar="timeout",
                    help="""set timeout for test execution timeout:
floating point number with an optional suffix:
's' for seconds, 'm' for minutes, 'h' for hours or 'd' for days.
Default value is 3 minutes.""",
                    default="3m")
parser.add_argument("-u",
                    metavar="test-sequence",
                    help="""run only tests from specified test sequence
e.g.: 0-2,5 will execute TEST0, TEST1, TEST2 and TEST5""",
                    default='')

# set_folders -- contains data which are unknown
arg, set_folders = parser.parse_known_args()
holder = []

# modules -- contains all TESTS.py files sorted alphabetically
modules = sorted(glob.glob('**/TESTS.py', recursive=True))

# search through set_folders and find correct directories
for i in set_folders:
    i = i.replace(".", "")
    i = i.replace(str(Path("/")), "")
    i = i + "/"
    for m in modules:
        if m.startswith(i):
            holder.append(i)

# if set_folders contains any correct data, then overwrite modules with them
if len(set_folders) > 0:
    modules = holder

# set global variables, based on given parameters
env_build = arg.b
env_timeout = arg.o
env_testseq = arg.u
env_testfile = arg.s
env_fs = arg.f
env_trace = arg.t
