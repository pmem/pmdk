#
# src/test/testconfig.py -- configuration for local and remote unit tests
#

config = {

    #
    #       1) *** LOCAL CONFIGURATION ***
    #
    #       The first part of the file tells the script unittest/unittest.py
    # which file system locations are to be used for local testing.
    #
    #
    # Changes logging level. Possible values:
    # 0 - silent (only error messages)
    # 1 - normal (above + SETUP + START + DONE + PASS + SKIP messages)
    # 2 - verbose (above + all SKIP messages + stdout from test binaries)
    #

    'unittest_log_level': 1,

    #
    # For tests that require true persistent memory, set the path to
    # a directory on a PMEM-aware file system here.
    # Comment this line out if there's no actual persistent memory
    #  available on this system.
    #

    'pmem_fs_dir': '/mnt/pmem',

    #
    # For tests that require true a non-persitent memory aware file system
    # (i.e. to verify something works on traditional page-cache based
    # memory-mapped files) set the path to a directory
    # on a normal file system here.
    #

    'non_pmem_fs_dir': '/tmp',

    #
    # To display execution time of each test
    #

    'tm': 1,

    #
    # Overwrite default test type:
    # check (default), short, medium, long, all
    # where: check = short + medium; all = short + medium + long
    #

    'test_type': 'check',

    #
    # Overwrite available build types:
    # debug, nondebug, static-debug, static-nondebug, all (default)
    #

    'test_build': 'all',

    #
    # Overwrite available filesystem types:
    # pmem, non-pmem, any, none, all (default)
    #

    'test_fs': 'all',

    #
    # Normally the first failed test terminates the test run. If KEEP_GOING
    # is set, continues executing all tests.
    #

    'keep_going': 'y',

    #
    # Overwrite default timeout
    # (floating point number with an optional suffix: 's' for seconds,
    # 'm' for minutes, 'h' for hours or 'd' for days)
    #
    'test_timeout': '3m',
    }
