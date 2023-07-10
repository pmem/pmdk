#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# RUNTESTS.sh -- setup the environment and run each test
#

#
# usage -- print usage message and exit
#
usage()
{
	[ "$1" ] && echo Error: $1
	cat >&2 <<EOF
Usage: $0 [ -hnv ] [ -b build-type ] [ -t test-type ] [ -f fs-type ]
		[ -o timeout ] [ -s test-file | -u test-sequence ] [-k skip-dir ]
		[[ -m memcheck ] [-p pmemcheck ] [ -e helgrind ] [ -d drd ] ||
		[ --force-enable memcheck|pmemcheck|helgrind|drd ]]
		[ -c ] [tests...]
-h			print this help message
-n			dry run
-v			be verbose
-b build-type		run only specified build type
			build-type: debug, nondebug, static_debug, static_nondebug, all (default)
-t test-type		run only specified test type
			test-type: check (default), short, medium, long, all
			where: check = short + medium; all = short + medium + long
-k skip-dir		skip a specific test directory
-f fs-type		run tests only on specified file systems
			fs-type: pmem, non-pmem, any, none, all (default)
-o timeout		set timeout for test execution
			timeout: floating point number with an optional suffix: 's' for seconds
			(the default), 'm' for minutes, 'h' for hours or 'd' for days.
			Default value is 3 minutes.
-s test-file		run only specified test file
			test-file: all (default), TEST0, TEST1, ...
-u test-sequence	run only tests from specified test sequence
			e.g.: 0-2,5 will execute TEST0, TEST1, TEST2 and TEST5
-m memcheck		run tests with memcheck
			memcheck: auto (default, enable/disable based on test requirements),
			force-enable (enable when test does not require memcheck, but
			obey test's explicit memcheck disable)
-p pmemcheck		run tests with pmemcheck
			pmemcheck: auto (default, enable/disable based on test requirements),
			force-enable (enable when test does not require pmemcheck, but
			obey test's explicit pmemcheck disable)
-e helgrind		run tests with helgrind
			helgrind: auto (default, enable/disable based on test requirements),
			force-enable (enable when test does not require helgrind, but
			obey test's explicit helgrind disable)
-d drd			run tests with drd
			drd: auto (default, enable/disable based on test requirements),
			force-enable (enable when test does not require drd, but
			obey test's explicit drd disable)
--force-enable memcheck|pmemcheck|helgrind|drd
			allows to force the use of a specific valgrind tool,
			but skips tests where the tool is explicitly disabled
			Can not be use with -m, -p, -e, -d.
-c			check pool files with pmempool check utility
EOF
	exit 1
}

#
# runtest_local -- run test using provided parameters
#
runtest_local() {
	local verbose_old=-1
	for vt in ${verbose_tests//:/ }; do
		[ "$RUNTEST_DIR" == "$vt" ] && {
			verbose_old=$verbose
			verbose=1
		}
	done

	if [ "$dryrun" ]
	then
		echo "(in ./$RUNTEST_DIR) $RUNTEST_PARAMS ./$RUNTEST_SCRIPT"
	elif [ "$use_timeout" -a "$testtype" = "check" ]
	then
		# set timeout for "check" tests
		[ "$verbose" ] && echo "RUNTESTS.sh: Running: (in ./$RUNTEST_DIR) \
			$RUNTEST_PARAMS ./$RUNTEST_SCRIPT"
		CMD_STR="$RUNTEST_EXTRA VERBOSE=$verbose $RUNTEST_PARAMS timeout \
			--foreground $killopt $RUNTEST_TIMEOUT ./$RUNTEST_SCRIPT"
		eval "$CMD_STR"
	else
		[ "$verbose" ] && echo "RUNTESTS.sh: Running: (in ./$RUNTEST_DIR) $params ./$script"
		CMD_STR="$RUNTEST_EXTRA VERBOSE=$verbose $RUNTEST_PARAMS ./$RUNTEST_SCRIPT"
		eval "$CMD_STR"
	fi

	retval=$?
	errmsg='failed'
	[ $retval = 124 -o $retval = 137 ] && errmsg='timed out'
	[ $retval != 0 ] && {
		[ -t 2 ] && command -v tput >/dev/null && errmsg="$(tput setaf 1)$errmsg$(tput sgr0)"
		echo "RUNTESTS.sh: stopping: $RUNTEST_DIR/$RUNTEST_SCRIPT $errmsg, $RUNTEST_PARAMS" >&2
		if [ "$keep_going" == "y" ]; then
			keep_going_exit_code=1
			keep_going_skip=y
			fail_list="$fail_list $RUNTEST_DIR/$RUNTEST_SCRIPT"
			((fail_count+=1))

			if [ "$CLEAN_FAILED" == "y" ]; then
				dir_rm=$(<$TEMP_LOC)
				rm -Rf $dir_rm
				if [ $? -ne 0 ]; then
					echo -e "Cannot remove directory with data: $dir_rm"
				fi
			fi
		else
			exit 1
		fi
	}
	rm -f $TEMP_LOC

	[ "$verbose_old" != "-1" ] && verbose=$verbose_old

	return 0
}

#
# load_default_global_test_configuration -- load a default global configuration
#
load_default_global_test_configuration() {
	global_req_testtype=all
	global_req_fstype=all
	global_req_buildtype=all
	global_req_timeout='3m'

	return 0
}

# switch_hyphen -- substitute hyphen for underscores
switch_hyphen() {
	echo ${1//-/_}
}

#
# read_global_test_configuration -- read a global configuration from a test
#	config file and overwrite a global configuration
#
read_global_test_configuration() {
	if [ ! -e "config.sh" ]; then
		return
	fi

	# unset all global settings
	unset CONF_GLOBAL_TEST_TYPE
	unset CONF_GLOBAL_FS_TYPE
	unset CONF_GLOBAL_BUILD_TYPE
	unset CONF_GLOBAL_TIMEOUT

	# unset all local settings
	unset CONF_TEST_TYPE
	unset CONF_FS_TYPE
	unset CONF_BUILD_TYPE
	unset CONF_TIMEOUT

	. config.sh

	[ -n "$CONF_GLOBAL_TEST_TYPE" ] && global_req_testtype=$CONF_GLOBAL_TEST_TYPE
	[ -n "$CONF_GLOBAL_FS_TYPE" ] && global_req_fstype=$CONF_GLOBAL_FS_TYPE
	[ -n "$CONF_GLOBAL_BUILD_TYPE" ] && global_req_buildtype=$CONF_GLOBAL_BUILD_TYPE
	[ -n "$CONF_GLOBAL_TIMEOUT" ] && global_req_timeout=$CONF_GLOBAL_TIMEOUT

	return 0
}

#
# read_test_configuration -- generate a test configuration from a global
#	configuration and a test configuration read from a test config file
#	usage: read_test_configuration <test-id>
#
read_test_configuration() {
	req_testtype=$global_req_testtype
	req_fstype=$global_req_fstype
	req_buildtype=$global_req_buildtype
	req_timeout=$global_req_timeout

	[ -n "${CONF_TEST_TYPE[$1]}" ] && req_testtype=${CONF_TEST_TYPE[$1]}
	[ -n "${CONF_FS_TYPE[$1]}" ] && req_fstype=${CONF_FS_TYPE[$1]}
	[ -n "${CONF_BUILD_TYPE[$1]}" ] && req_buildtype=${CONF_BUILD_TYPE[$1]}
	if [ -n "$runtest_timeout" ]; then
		req_timeout="$runtest_timeout"
	else
		[ -n "${CONF_TIMEOUT[$1]}" ] && req_timeout=${CONF_TIMEOUT[$1]}
	fi

	special_params=
	[ "$req_fstype" == "none" -o "$req_fstype" == "any" ] && \
		special_params="req_fs_type=1"

	return 0
}

#
# intersection -- return common elements of collection of available and required
#	values
#	usage: intersection <available> <required> <complete-collection>
#
intersection() {
	collection=$1
	[ "$collection" == "all" ] && collection=$3
	[ "$2" == "all" ] && echo $collection && return
	for e in $collection; do
		for r in $2; do
			[ "$e" == "$r" ] && {
				subset="$subset $e"
			}
		done
	done
	echo $subset
}

#
# runtest -- given the test directory name, run tests found inside it
#
runtest() {
	[ "$UNITTEST_LOG_LEVEL" ] || UNITTEST_LOG_LEVEL=1
	export UNITTEST_LOG_LEVEL

	[ -f "$1/TEST0" ] || {
		echo FAIL: $1: test not found. >&2
		exit 1
	}
	[ -x "$1/TEST0" ] || {
		echo FAIL: $1: test not executable. >&2
		exit 1
	}

	cd $1

	load_default_global_test_configuration
	read_global_test_configuration

	runscripts=$testfile
	if [ "$runscripts" = all ]; then
		if [ "$testseq" = all ]; then
			runscripts=`ls -1 TEST* | grep '^TEST[0-9]\+$' | sort -V`
		else
			# generate test sequence
			seqs=(${testseq//,/ })
			runscripts=
			for seq in ${seqs[@]}; do
				limits=(${seq//-/ })
				if [ "${#limits[@]}" -eq "2" ]; then
					if [ ${limits[0]} -lt ${limits[1]} ]; then
						nos="$(seq ${limits[0]} ${limits[1]})"
					else
						nos="$(seq ${limits[1]} ${limits[0]})"
					fi
				else
					nos=${limits[0]}
				fi
				for no in $nos; do
					runscripts="$runscripts TEST$no"
				done
			done
		fi
	fi

	# for each TEST script found...
	for runscript in $runscripts
	do
		UNITTEST_NAME="$1/$runscript"
		local sid=${runscript#TEST}
		read_test_configuration $sid

		local _testtype="$testtype"
		# unwind check test type to its subtypes
		[ "$_testtype" == "check" ] && _testtype="short medium"
		[ "$_testtype" == "all" ] && _testtype="short medium long"

		ttype=$(intersection "$_testtype" "$req_testtype" "short medium long")
		[ -z "$ttype" ] && {
			echo "$UNITTEST_NAME: SKIP test-type $testtype ($req_testtype required)"
			continue
		}
		# collapse test type to check if its valid superset
		[ "$ttype" == "short medium" ] && ttype="check"
		[ "$ttype" == "short medium long" ] && ttype="all"

		# check if output test type is single value
		ttype_array=($ttype)
		[ "${#ttype_array[@]}" -gt 1 ] && {
			echo "$UNITTEST_NAME: multiple test types ($ttype)"
			exit 1
		}

		fss=$(intersection "$fstype" "$req_fstype" "none pmem non-pmem any")
		builds=$(intersection "$buildtype" "$req_buildtype" "debug nondebug static_debug static_nondebug")

		# for each fs-type being tested...
		for fs in $fss
		do
			# don't bother trying when fs-type isn't available...
			if [ "$fs" == "pmem" ] && [ -z "$PMEM_FS_DIR" ] && [ "$fstype" == "all" ]; then
				pmem_skip=1
				continue
			fi

			if [ "$fs" == "non-pmem" ] && [ -z "$NON_PMEM_FS_DIR" ] && [ "$fstype" == "all" ]; then
				non_pmem_skip=1
				continue
			fi

			if [ "$fs" == "any" ] && [ -z "$PMEM_FS_DIR" ] && [ -z "$NON_PMEM_FS_DIR" ] && [ "$fstype" == "all" ]; then
				continue
			fi
			# for each build-type being tested...
			for build in $builds
			do
				export RUNTEST_DIR=$1
				export RUNTEST_PARAMS="TEST=$ttype FS=$fs BUILD=$build"
				export RUNTEST_EXTRA="CHECK_TYPE=$checktype \
					FORCE_CHECK_TYPE=$checktype \
					CHECK_POOL=$check_pool \
					$special_params"
				export RUNTEST_SCRIPT="$runscript"
				export RUNTEST_TIMEOUT="$req_timeout"

				if [ "$KEEP_GOING" == "y" ] && [ "$CLEAN_FAILED" == "y" ]; then
					# temporary file used for sharing data
					# between RUNTESTS.sh and tests processes
					temp_loc=$(mktemp /tmp/data-location.XXXXXXXX)
					export TEMP_LOC=$temp_loc
				fi
				# to not overwrite logs skip other tests from the group
				# if KEEP_GOING=y and test fail
				if [ "$keep_going_skip" == "n" ]; then
					runtest_local
				fi
			done
		done
		keep_going_skip=n
	done

	cd ..
}

[ -f testconfig.sh ] || {
	cat >&2 <<EOF
RUNTESTS.sh: stopping because no testconfig.sh is found.
		  to create one:
			   cp testconfig.sh.example testconfig.sh
		  and edit testconfig.sh to describe the local machine configuration.
EOF
	exit 1
}

. ./testconfig.sh

#
# defaults...
#
def_buildtype=all
testtype=check
fstype=all
testconfig="./testconfig.sh"
killopt="-k 10s"
runtest_timeout="3m"
use_timeout="ok"
testfile=all
testseq=all
check_pool=0
checktype="none"
skip_dir=""
keep_going=n
keep_going_skip=n
keep_going_exit_code=0
fail_count=0
fail_list=""
verbose_tests=

#
# some of defaults can be overwritten with environment variables
# (placed e.g. in testconfig.sh)
#
[ -n "$TEST_BUILD" ] && def_buildtype=$TEST_BUILD
[ -n "$TEST_TYPE" ] && testtype=$TEST_TYPE
[ -n "$TEST_FS" ] && fstype=$TEST_FS
[ -n "$TEST_TIMEOUT" ] && runtest_timeout=$TEST_TIMEOUT
[ -n "$KEEP_GOING" ] && keep_going=$KEEP_GOING
[ -n "$VERBOSE_TESTS" ] && verbose_tests="$VERBOSE_TESTS"

PMEMDETECT="tools/pmemdetect/pmemdetect.static_nondebug"
pmemdetect() {
	LD_LIBRARY_PATH=$LIBNDCTL_LD_LIBRARY_PATHS:$LD_LIBRARY_PATH $PMEMDETECT "$@"
}

if [ "$PMEM_FS_DIR_FORCE_PMEM" = "1" ]; then
	if pmemdetect -s $PMEM_FS_DIR; then
		echo "error: PMEM_FS_DIR_FORCE_PMEM variable is set but PMEM_FS_DIR [" $PMEM_FS_DIR  "] supports MAP_SYNC"
		echo "Setting this flag prevents from testing an integration between pmem_map_file and pmem_is_pmem."
		echo "If you want to ignore this error, please set PMEM_FS_DIR_FORCE_PMEM=2."
		exit 1
	fi
fi

if [ -v PMEM_FS_DIR ] && [ ! -d "$PMEM_FS_DIR" ]; then
	echo "error: PMEM_FS_DIR=$PMEM_FS_DIR doesn't exist"
	exit 1
fi

if [ -v NON_PMEM_FS_DIR ] && [ ! -d "$NON_PMEM_FS_DIR" ]; then
	echo "error: NON_PMEM_FS_DIR=$NON_PMEM_FS_DIR doesn't exist"
	exit 1
fi

if [ -d "$PMEM_FS_DIR" ]; then
	if [ "$PMEM_FS_DIR_FORCE_PMEM" = "1" ] || [ "$PMEM_FS_DIR_FORCE_PMEM" = "2" ]; then
		PMEM_IS_PMEM=0
	else
		pmemdetect "$PMEM_FS_DIR"
		PMEM_IS_PMEM=$?
	fi

	if [ $PMEM_IS_PMEM -ne 0 ]; then
		echo "error: PMEM_FS_DIR=$PMEM_FS_DIR does not point to a PMEM device"
		exit 1
	fi
fi

if [ -d "$NON_PMEM_FS_DIR" ]; then
	pmemdetect "$NON_PMEM_FS_DIR"
	NON_PMEM_IS_PMEM=$?
	if [ $NON_PMEM_IS_PMEM -eq 0 ]; then
		echo "error: NON_PMEM_FS_DIR=$NON_PMEM_FS_DIR does not point to a non-PMEM device"
		exit 1
	fi
fi

#
# command-line argument processing...
#
args=`getopt --unquoted --longoptions force-enable: k:nvb:t:f:o:s:u:m:e:p:d:cq:r:g:x: $*`
[ $? != 0 ] && usage
set -- $args
for arg
do
	receivetype=auto
	case "$arg"
	in
	--force-enable)
		receivetype="$2"
		shift 2
		if [ "$checktype" != "none" ]; then
			usage "cannot force-enable two test types at the same time"
		fi

		case "$receivetype"
		in
		memcheck|pmemcheck|helgrind|drd)
			;;
		*)
			usage "bad force-enable: $receivetype"
			;;
		esac
		checktype=$receivetype
		;;
	-k)
		skip_dir="$skip_dir $2"
		shift 2
		;;
	-n)
		dryrun=1
		shift
		;;
	-v)
		verbose=1
		shift
		;;
	-b)
		buildtype="$buildtype $2"
		case "$2"
		in
		debug|nondebug|static_debug|static_nondebug|all)
			;;
		*)
			usage "bad build-type: $buildtype"
			;;
		esac
		shift 2
		;;
	-t)
		testtype="$2"
		shift 2
		case "$testtype"
		in
		short|medium|long|check|all)
			;;
		*)
			usage "bad test-type: $testtype"
			;;
		esac
		;;
	-f)
		fstype="$2"
		shift 2
		case "$fstype"
		in
		none|pmem|non-pmem|any)
			# necessary to treat "any" as either pmem or non-pmem with "-f"
			export FORCE_FS=1
			;;
		all)
			;;
		*)
			usage "bad fs-type: $fstype"
			;;
		esac
		;;
	-m)
		receivetype="$2"
		shift 2
		case "$receivetype"
		in
		auto)
			;;
		force-enable)
			if [ "$checktype" != "none" ]; then
				usage "cannot force-enable two test types at the same time"
			fi

			checktype="memcheck"
			;;
		*)
			usage "bad memcheck: $receivetype"
			;;
		esac
		;;
	-p)
		receivetype="$2"
		shift 2
		case "$receivetype"
		in
		auto)
			;;
		force-enable)
			if [ "$checktype" != "none" ]; then
				usage "cannot force-enable two test types at the same time"
			fi

			checktype="pmemcheck"
			;;
		*)
			usage "bad pmemcheck: $receivetype"
			;;
		esac
		;;
	-e)
		receivetype="$2"
		shift 2
		case "$receivetype"
		in
		auto)
			;;
		force-enable)
			if [ "$checktype" != "none" ]; then
				usage "cannot force-enable two test types at the same time"
			fi

			checktype="helgrind"
			;;
		*)
			usage "bad helgrind: $receivetype"
			;;
		esac
		;;
	-d)
		receivetype="$2"
		shift 2
		case "$receivetype"
		in
		auto)
			;;
		force-enable)
			if [ "$checktype" != "none" ]; then
				usage "cannot force-enable two test types at the same time"
			fi

			checktype="drd"
			;;
		*)
			usage "bad drd: $receivetype"
			;;
		esac
		;;
	-o)
		runtest_timeout="$2"
		shift 2
		;;
	-s)
		testfile="$2"
		testseq=all
		shift 2
		;;

	-u)
		testseq="$2"
		testfile=all
		shift 2
		;;
	-c)
		check_pool=1
		shift
		;;
	--)
		shift
		break
		;;
	esac
done

[ -z "$buildtype" ] && buildtype=$def_buildtype
[[ $buildtype =~ .*all.* ]] && buildtype=all

# parse MAKEFLAGS variable
[ -n "$MAKEFLAGS" ] && {
	# extract flags from variable
	FLAGS=
	for flag in $MAKEFLAGS; do
		[ "$flag" == "--" ] && break
		FLAGS+="$flag"
	done

	[ -n "$FLAGS" ] && {
		# apply supported flags
		for i in $(seq ${#FLAGS}); do
			case "${FLAGS:i-1:1}"
			in
			k)
				keep_going=y
				;;
			esac
		done
	}
}

[ "$verbose" ] && {
	echo -n Options:
	[ "$dryrun" ] && echo -n ' -n'
	[ "$verbose" ] && echo -n ' -v'
	echo
	echo "    build-type: $buildtype"
	echo "    test-type: $testtype"
	echo "    fs-type: $fstype"
	echo "    check-type: $checktype"
	if [ "$check_pool" ]
	then
		check_pool_str="yes"
	else
		check_pool_str="no"
	fi
	echo "    check-pool: $check_pool_str"
	echo "    skip-dir: $skip_dir"
	echo Tests: $*
}

# check if timeout supports "-k" option
timeout -k 1s 1s true &>/dev/null
if [ $? != 0 ]; then
	unset killopt
fi

# check if timeout can be run in the foreground
timeout --foreground 1s true &>/dev/null
if [ $? != 0 ]; then
	unset use_timeout
fi

if [ -n "$TRACE" ]; then
	unset use_timeout
fi

if [ "$1" ]; then
	for test in $*
	do
		[ -d "$test" ] || echo "RUNTESTS.sh: Test does not exist: $test"
		[ -f "$test/TEST0" ] && runtest $test
	done
else
	# no arguments means run them all
	for testfile0 in */TEST0
	do
		testdir=`dirname $testfile0`
		if [[ "$skip_dir" =~ "$testdir" ]]; then
			echo "RUNTESTS.sh: Skipping: $testdir"
			continue
		fi
		runtest $testdir
	done
fi

[ "$pmem_skip" ] && echo "SKIPPED fs-type \"pmem\" runs: testconfig.sh doesn't set PMEM_FS_DIR"
[ "$non_pmem_skip" ] && echo "SKIPPED fs-type \"non-pmem\" runs: testconfig.sh doesn't set NON_PMEM_FS_DIR"

if [ "$fail_count" != "0" ]; then
	echo "$(tput setaf 1)$fail_count tests failed:$(tput sgr0)"
	# remove duplicates and print each test name in a new line
	echo $fail_list | xargs -n1 | uniq
	exit $keep_going_exit_code
else
	exit 0
fi
