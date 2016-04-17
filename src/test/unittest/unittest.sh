#
# Copyright 2014-2016, Intel Corporation
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

. ../testconfig.sh

# defaults
[ "$TEST" ] || export TEST=check
[ "$FS" ] || export FS=any
[ "$BUILD" ] || export BUILD=debug
[ "$CHECK_TYPE" ] || export CHECK_TYPE=auto
[ "$CHECK_POOL" ] || export CHECK_POOL=0
[ "$VERBOSE" ] || export VERBOSE=0

TOOLS=../tools
# Paths to some useful tools
[ "$PMEMPOOL" ] || PMEMPOOL=../../tools/pmempool/pmempool
[ "$PMEMSPOIL" ] || PMEMSPOIL=$TOOLS/pmemspoil/pmemspoil.static-nondebug
[ "$PMEMWRITE" ] || PMEMWRITE=$TOOLS/pmemwrite/pmemwrite
[ "$PMEMALLOC" ] || PMEMALLOC=$TOOLS/pmemalloc/pmemalloc
[ "$PMEMDETECT" ] || PMEMDETECT=$TOOLS/pmemdetect/pmemdetect.static-nondebug

# force globs to fail if they don't match
shopt -s failglob

#
# For non-static build testing, the variable TEST_LD_LIBRARY_PATH is
# constructed so the test pulls in the appropriate library from this
# source tree.  To override this behavior (i.e. to force the test to
# use the libraries installed elsewhere on the system), set
# TEST_LD_LIBRARY_PATH and this script will not override it.
#
# For example, in a test directory, run:
#	TEST_LD_LIBRARY_PATH=/usr/lib ./TEST0
#
[ "$TEST_LD_LIBRARY_PATH" ] || {
	case "$BUILD"
	in
	debug)
		TEST_LD_LIBRARY_PATH=../../debug
		;;
	nondebug)
		TEST_LD_LIBRARY_PATH=../../nondebug
		;;
	esac
}

#
# When running static binary tests, append the build type to the binary
#
case "$BUILD"
in
	static-*)
		EXESUFFIX=.$BUILD
		;;
esac

#
# The variable DIR is constructed so the test uses that directory when
# constructing test files.  DIR is chosen based on the fs-type for
# this test, and if the appropriate fs-type doesn't have a directory
# defined in testconfig.sh, the test is skipped.
#
# This behavior can be overridden by setting DIR.  For example:
#	DIR=/force/test/dir ./TEST0
#
curtestdir=`basename $PWD`

# just in case
if [ ! "$curtestdir" ]; then
	echo "curtestdir does not have a value" >&2
	exit 1
fi

curtestdir=test_$curtestdir

if [ ! "$UNITTEST_NUM" ]; then
	echo "UNITTEST_NUM does not have a value" >&2
	exit 1
fi

if [ ! "$UNITTEST_NAME" ]; then
	echo "UNITTEST_NAME does not have a value" >&2
	exit 1
fi

REAL_FS=$FS
if [ "$DIR" ]; then
	DIR=$DIR/$curtestdir$UNITTEST_NUM
else
	case "$FS"
	in
	pmem)
		DIR=$PMEM_FS_DIR/$curtestdir$UNITTEST_NUM
		if [ "$PMEM_FS_DIR_FORCE_PMEM" = "1" ]; then
			export PMEM_IS_PMEM_FORCE=1
		fi
		;;
	non-pmem)
		DIR=$NON_PMEM_FS_DIR/$curtestdir$UNITTEST_NUM
		;;
	any)
		if [ "$PMEM_FS_DIR" != "" ]; then
			DIR=$PMEM_FS_DIR/$curtestdir$UNITTEST_NUM
			REAL_FS=pmem
			if [ "$PMEM_FS_DIR_FORCE_PMEM" = "1" ]; then
				export PMEM_IS_PMEM_FORCE=1
			fi
		elif [ "$NON_PMEM_FS_DIR" != "" ]; then
			DIR=$NON_PMEM_FS_DIR/$curtestdir$UNITTEST_NUM
			REAL_FS=non-pmem
		else
			echo "$UNITTEST_NAME: fs-type=any and both env vars are empty" >&2
			exit 1
		fi
		;;
	none)
		DIR=/dev/null/not_existing_dir/$curtestdir$UNITTEST_NUM
		;;
	*)
		[ "$UNITTEST_QUIET" ] || echo "$UNITTEST_NAME: SKIP fs-type $FS (not configured)"
		exit 0
                ;;
        esac
fi

if [ -d "$PMEM_FS_DIR" ]; then
	if [ "$PMEM_FS_DIR_FORCE_PMEM" = "1" ]; then
		PMEM_IS_PMEM=0
	else
		$PMEMDETECT "$PMEM_FS_DIR" && true
		PMEM_IS_PMEM=$?
	fi
fi

if [ -d "$NON_PMEM_FS_DIR" ]; then
	$PMEMDETECT "$NON_PMEM_FS_DIR" && true
	NON_PMEM_IS_PMEM=$?
fi

#
# The default is to turn on library logging to level 3 and save it to local files.
# Tests that don't want it on, should override these environment variables.
#
export VMEM_LOG_LEVEL=3
export VMEM_LOG_FILE=vmem$UNITTEST_NUM.log
export PMEM_LOG_LEVEL=3
export PMEM_LOG_FILE=pmem$UNITTEST_NUM.log
export PMEMBLK_LOG_LEVEL=3
export PMEMBLK_LOG_FILE=pmemblk$UNITTEST_NUM.log
export PMEMLOG_LOG_LEVEL=3
export PMEMLOG_LOG_FILE=pmemlog$UNITTEST_NUM.log
export PMEMOBJ_LOG_LEVEL=3
export PMEMOBJ_LOG_FILE=pmemobj$UNITTEST_NUM.log

export VMMALLOC_POOL_DIR="$DIR"
export VMMALLOC_POOL_SIZE=$((16 * 1024 * 1024))
export VMMALLOC_LOG_LEVEL=3
export VMMALLOC_LOG_FILE=vmmalloc$UNITTEST_NUM.log

export VALGRIND_LOG_FILE=${CHECK_TYPE}${UNITTEST_NUM}.log
export VALIDATE_VALGRIND_LOG=1

[ "$UT_DUMP_LINES" ] || UT_DUMP_LINES=30

export CHECK_POOL_LOG_FILE=check_pool_${BUILD}_${UNITTEST_NUM}.log

#
# create_file -- create zeroed out files of a given length in megs
#
# example, to create two files, each 1GB in size:
#	create_file 1024 testfile1 testfile2
#
function create_file() {
	size=$1
	shift
	for file in $*
	do
		dd if=/dev/zero of=$file bs=1M count=$size >> prep$UNITTEST_NUM.log
	done
}

#
# create_nonzeroed_file -- create non-zeroed files of a given length in megs
#
# A given first kilobytes of the file is zeroed out.
#
# example, to create two files, each 1GB in size, with first 4K zeroed
#	create_nonzeroed_file 1024 4 testfile1 testfile2
#
function create_nonzeroed_file() {
	offset=$2
	size=$(($1 * 1024 - $offset))
	shift 2
	for file in $*
	do
		truncate -s ${offset}K $file >> prep$UNITTEST_NUM.log
		dd if=/dev/zero bs=1K count=${size} 2>>prep$UNITTEST_NUM.log | tr '\0' '\132' >> $file
	done
}

#
# create_holey_file -- create holey files of a given length in megs
#
# example, to create two files, each 1GB in size:
#	create_holey_file 1024 testfile1 testfile2
#
function create_holey_file() {
	size=$1
	shift
	for file in $*
	do
		truncate -s ${size}M $file >> prep$UNITTEST_NUM.log
	done
}

#
# create_poolset -- create a dummy pool set
#
# Creates a pool set file using the provided list of part sizes and paths.
# Optionally, it also creates the selected part files (zeroed, partially zeroed
# or non-zeroed) with requested size and mode.  The actual file size may be
# different than the part size in the pool set file.
# 'r' or 'R' on the list of arguments indicate the beginning of the next
# replica set.
#
# Each part argument has the following format:
#   psize:ppath[:cmd[:fsize[:mode]]]
#
# where:
#   psize - part size
#   ppath - path
#   cmd   - (optional) can be:
#            x - do nothing (may be skipped if there's no 'fsize', 'mode')
#            z - create zeroed (holey) file
#            n - create non-zeroed file
#            h - create non-zeroed file, but with zeroed header (first 4KB)
#   fsize - (optional) the actual size of the part file (if 'cmd' is not 'x')
#   mode  - (optional) same format as for 'chmod' command
#
# example:
#   The following command define a pool set consisting of two parts: 16MB
#   and 32MB, and the replica with only one part of 48MB.  The first part file
#   is not created, the second is zeroed.  The only replica part is non-zeroed.
#   Also, the last file is read-only and its size does not match the information
#   from pool set file.
#
#	create_poolset ./pool.set 16M:testfile1 32M:testfile2:z \
#				R 48M:testfile3:n:11M:0400
#
function create_poolset() {
	psfile=$1
	shift 1
	echo "PMEMPOOLSET" > $psfile
	while [ "$1" ]
	do
		if [ "$1" = "R" ] || [ "$1" = "r" ]
		then
			echo "REPLICA" >> $psfile
			shift 1
			continue
		fi

		cmd=$1
		fparms=(${cmd//:/ })
		shift 1

		fsize=${fparms[0]}
		fpath=`readlink -mn ${fparms[1]}`
		cmd=${fparms[2]}
		asize=${fparams[3]}
		mode=${fparms[4]]}

		if [ ! $asize ]; then
			asize=$fsize
		fi

		case "$cmd"
		in
		x)
			# do nothing
			;;
		z)
			# zeroed (holey) file
			truncate -s $asize $fpath >> prep$UNITTEST_NUM.log
			;;
		n)
			# non-zeroed file
			dd if=/dev/zero bs=$asize count=1 2>>prep$UNITTEST_NUM.log | tr '\0' '\132' >> $fpath
			;;
		h)
			# non-zeroed file, except 4K header
			truncate -s 4K $fpath >> prep$UNITTEST_NUM.log
			dd if=/dev/zero bs=$asize count=1 2>>prep$UNITTEST_NUM.log | tr '\0' '\132' >> $fpath
			truncate -s $asize $fpath >> prep$UNITTEST_NUM.log
			;;
		esac

		if [ $mode ]; then
			chmod $mode $fpath
		fi

		echo "$fsize $fpath" >> $psfile
	done
}

function dump_last_n_lines() {
	if [ -f $1 ]; then
		ln=`wc -l < $1`
		if [ $ln -gt $UT_DUMP_LINES ]; then
			echo -e "Last $UT_DUMP_LINES lines of $1 below (whole file has $ln lines)." >&2
			ln=$UT_DUMP_LINES
		else
			echo -e "$1 below." >&2
		fi
		paste -d " " <(yes $UNITTEST_NAME $1 | head -n $ln) <(tail -n $ln $1) >&2
		echo >&2
	fi
}

# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=810295
# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=780173
# https://bugs.kde.org/show_bug.cgi?id=303877
function ignore_debug_info_errors() {
	cat $1 | grep -v \
		-e "WARNING: Serious error when reading debug info" \
		-e "When reading debug info from " \
		-e "Ignoring non-Dwarf2/3/4 block in .debug_info" \
		-e "Last block truncated in .debug_info; ignoring" \
		-e "parse_CU_Header: is neither DWARF2 nor DWARF3 nor DWARF4" \
		>  $1.tmp
	mv $1.tmp $1
}

#
# expect_normal_exit -- run a given command, expect it to exit 0
#
function expect_normal_exit() {
	if [ "$CHECK_TYPE" != "none" ]; then
		OLDTRACE="$TRACE"
		rm -f $VALGRIND_LOG_FILE
		if [ "$CHECK_TYPE" = "memcheck" -a "$MEMCHECK_DONT_CHECK_LEAKS" != "1" ]; then
			export OLD_VALGRIND_OPTS="$VALGRIND_OPTS"
			export VALGRIND_OPTS="$VALGRIND_OPTS --leak-check=full"
		fi
		export VALGRIND_OPTS="$VALGRIND_OPTS --suppressions=../ld.supp"
		TRACE="$VALGRINDEXE --tool=$CHECK_TYPE --log-file=$VALGRIND_LOG_FILE $VALGRIND_OPTS $TRACE"
	fi

	if [ "$MEMCHECK_DONT_CHECK_LEAKS" = "1" -a "$CHECK_TYPE" = "memcheck" ]; then
		export OLD_ASAN_OPTIONS="${ASAN_OPTIONS}"
		export ASAN_OPTIONS="detect_leaks=0 ${ASAN_OPTIONS}"
	fi

	set +e
	eval $ECHO LD_LIBRARY_PATH=$TEST_LD_LIBRARY_PATH LD_PRELOAD=$TEST_LD_PRELOAD \
	$TRACE $*
	ret=$?
	set -e
	if [ "$ret" -ne "0" ]; then
		if [ "$ret" -gt "128" ]; then
			msg="crashed (signal $(($ret - 128)))"
		else
			msg="failed with exit code $ret"
		fi
		[ -t 2 ] && command -v tput >/dev/null && msg="$(tput setaf 1)$msg$(tput sgr0)"

		if [ -f err$UNITTEST_NUM.log ]; then
			if [ "$UNITTEST_QUIET" = "1" ]; then
				echo -e "$UNITTEST_NAME $msg. err$UNITTEST_NUM.log below." >&2
				cat err$UNITTEST_NUM.log >&2
			else
				echo -e "$UNITTEST_NAME $msg. err$UNITTEST_NUM.log above." >&2
			fi
		else
			echo -e "$UNITTEST_NAME $msg." >&2
		fi
		if [ "$CHECK_TYPE" != "none" ]; then
			echo "$VALGRIND_LOG_FILE below." >&2
			ln=`wc -l < $VALGRIND_LOG_FILE`
			paste -d " " <(yes $UNITTEST_NAME $VALGRIND_LOG_FILE | head -n $ln) <(head -n $ln $VALGRIND_LOG_FILE) >&2
		fi

		# ignore Ctrl-C
		if [ $ret != 130 ]; then
			dump_last_n_lines out$UNITTEST_NUM.log
			dump_last_n_lines $PMEM_LOG_FILE
			dump_last_n_lines $PMEMOBJ_LOG_FILE
			dump_last_n_lines $PMEMLOG_LOG_FILE
			dump_last_n_lines $PMEMBLK_LOG_FILE
			dump_last_n_lines $VMEM_LOG_FILE
			dump_last_n_lines $VMMALLOC_LOG_FILE
		fi

		[ $NODES_MAX -ge 0 ] && clean_all_remote_nodes

		false
	fi
	if [ "$CHECK_TYPE" != "none" ]; then
		TRACE="$OLDTRACE"
		if [ -f $VALGRIND_LOG_FILE -a "${VALIDATE_VALGRIND_LOG}" = "1" ]; then
			ignore_debug_info_errors ${VALGRIND_LOG_FILE}
			if [ ! -e $CHECK_TYPE$UNITTEST_NUM.log.match ] && grep "ERROR SUMMARY: [^0]" $VALGRIND_LOG_FILE >/dev/null; then
				msg="failed"
				[ -t 2 ] && command -v tput >/dev/null && msg="$(tput setaf 1)$msg$(tput sgr0)"
				echo -e "$UNITTEST_NAME $msg with Valgrind. See $VALGRIND_LOG_FILE. First 20 lines below." >&2
				paste -d " " <(yes $UNITTEST_NAME $VALGRIND_LOG_FILE | head -n 20) <(head -n 20 $VALGRIND_LOG_FILE) >&2
				false
			fi
		fi

		if [ "$CHECK_TYPE" = "memcheck" -a "$MEMCHECK_DONT_CHECK_LEAKS" != "1" ]; then
			export VALGRIND_OPTS="$OLD_VALGRIND_OPTS"
		fi
	fi
	if [ "$CHECK_TYPE" = "memcheck" -a "$MEMCHECK_DONT_CHECK_LEAKS" = "1" ]; then
		export ASAN_OPTIONS="${OLD_ASAN_OPTIONS}"
	fi
}

#
# expect_abnormal_exit -- run a given command, expect it to exit non-zero
#
function expect_abnormal_exit() {
	set +e
	eval $ECHO ASAN_OPTIONS="detect_leaks=0 ${ASAN_OPTIONS}" LD_LIBRARY_PATH=$TEST_LD_LIBRARY_PATH LD_PRELOAD=$TEST_LD_PRELOAD \
	$TRACE $*
	ret=$?
	set -e

	if [ "$ret" -eq "0" ]; then
		msg="succeeded"
		[ -t 2 ] && command -v tput >/dev/null && msg="$(tput setaf 1)$msg$(tput sgr0)"

		echo -e "$UNITTEST_NAME command $msg unexpectedly." >&2

		[ $NODES_MAX -ge 0 ] && clean_all_remote_nodes

		false
	fi
}

#
# check_pool -- run pmempool check on specified pool file
#
function check_pool() {
	if [ "$CHECK_POOL" == "1" ]
	then
		if [ "$VERBOSE" != "0" ]
		then
			echo "$UNITTEST_NAME: checking consistency of pool ${1}"
		fi
		${PMEMPOOL}.static-nondebug check $1 2>&1 1>>$CHECK_POOL_LOG_FILE
	fi
}

#
# check_pools -- run pmempool check on specified pool files
#
function check_pools() {
	if [ "$CHECK_POOL" == "1" ]
	then
		for f in $*
		do
			check_pool $f
		done
	fi
}

#
# require_unlimited_vm -- require unlimited virtual memory
#
# This implies requirements for:
# - overcommit_memory enabled (/proc/sys/vm/overcommit_memory is 0 or 1)
# - unlimited virtual memory (ulimit -v is unlimited)
#
function require_unlimited_vm() {
	local overcommit=$(cat /proc/sys/vm/overcommit_memory)
	local vm_limit=$(ulimit -v)
	[ "$overcommit" != "2" ] && [ "$vm_limit" = "unlimited" ] && return
	echo "$UNITTEST_NAME: SKIP required: overcommit_memory enabled and unlimited virtual memory"
	exit 0
}

#
# require_no_superuser -- require user without superuser rights
#
function require_no_superuser() {
	local user_id=$(id -u)
	[ "$user_id" != "0" ] && return
	echo "$UNITTEST_NAME: SKIP required: run without superuser rights"
	exit 0
}

#
# require_test_type -- only allow script to continue for a certain test type
#
function require_test_type() {
	for type in $*
	do
		[ "$type" = "$TEST" ] && return
	done
	[ "$UNITTEST_QUIET" ] || echo "$UNITTEST_NAME: SKIP test-type $TEST ($* required)"
	exit 0
}

#
# require_pmem -- only allow script to continue for a real PMEM device
#
function require_pmem() {
	[ $PMEM_IS_PMEM -eq 0 ] && return
	echo "error: PMEM_FS_DIR=$PMEM_FS_DIR does not point to a PMEM device"
	exit 1
}

#
# require_non_pmem -- only allow script to continue for a non-PMEM device
#
function require_non_pmem() {
	[ $NON_PMEM_IS_PMEM -ne 0 ] && return
	echo "error: NON_PMEM_FS_DIR=$NON_PMEM_FS_DIR does not point to a non-PMEM device"
	exit 1
}

#
# require_fs_type -- only allow script to continue for a certain fs type
#
function require_fs_type() {
	req_fs_type=1
	for type in $*
	do
		[ "$type" = "$FS" ] &&
		case "$REAL_FS"
		in
		pmem)
			require_pmem && return
			;;
		non-pmem)
			require_non_pmem && return
			;;
		none)
			return
			;;
		esac
	done
	[ "$UNITTEST_QUIET" ] || echo "$UNITTEST_NAME: SKIP fs-type $FS ($* required)"
	exit 0
}

#
# require_build_type -- only allow script to continue for a certain build type
#
function require_build_type() {
	for type in $*
	do
		[ "$type" = "$BUILD" ] && return
	done
	[ "$UNITTEST_QUIET" ] || echo "$UNITTEST_NAME: SKIP build-type $BUILD ($* required)"
	exit 0
}

#
# require_pkg -- only allow script to continue if specified package exists
#
function require_pkg() {
	if ! command -v pkg-config 1>/dev/null
	then
		echo "$UNITTEST_NAME: SKIP pkg-config required"
		exit 0
	fi

	if ! pkg-config $1
	then
		echo "$UNITTEST_NAME: SKIP '$1' package required"
		exit 0
	fi
}

#
# configure_valgrind -- only allow script to continue when settings match
#
function configure_valgrind() {
	case "$1"
	in
	memcheck|pmemcheck|helgrind|drd|force-disable)
		;;
	*)
		usage "bad test-type: $1"
		;;
	esac

	if [ "$CHECK_TYPE" == "none" ]; then
		if [ "$1" == "force-disable" ]; then
			echo "all valgrind tests disabled"
		elif [ "$2" = "force-enable" ]; then
			CHECK_TYPE="$1"
			require_valgrind_$1
		elif [ "$2" = "force-disable" ]; then
			CHECK_TYPE=none
		else
			echo "invalid parameter" >&2
			exit 1
		fi
	else
		if [ "$1" == "force-disable" ]; then
			echo "$UNITTEST_NAME: SKIP RUNTESTS script parameter $CHECK_TYPE tries to enable valgrind test when all valgrind tests are disabled in TEST"
			exit 0
		elif [ "$CHECK_TYPE" != "$1" -a "$2" == "force-enable" ]; then
			echo "$UNITTEST_NAME: SKIP RUNTESTS script parameter $CHECK_TYPE tries to enable different valgrind test than one defined in TEST"
			exit 0
		elif [ "$CHECK_TYPE" == "$1" -a "$2" == "force-disable" ]; then
			echo "$UNITTEST_NAME: SKIP RUNTESTS script parameter $CHECK_TYPE tries to enable test defined in TEST as force-disable"
			exit 0
		fi
		require_valgrind_$CHECK_TYPE
	fi
}

#
# require_valgrind -- continue script execution only if
#	valgrind package is installed
#
function require_valgrind() {
	require_no_asan
	VALGRINDEXE=`which valgrind 2>/dev/null` && return
	echo "$UNITTEST_NAME: SKIP valgrind package required"
	exit 0
}

#
# require_valgrind_pmemcheck -- continue script execution only if
#	valgrind with pmemcheck is installed
#
function require_valgrind_pmemcheck() {
	require_valgrind
	local binary=$1
	[ -n "$binary" ] || binary=*
        strings ${binary} 2>&1 | \
            grep -q "compiled with support for Valgrind pmemcheck" && true
        if [ $? -ne 0 ]; then
            echo "$UNITTEST_NAME: SKIP not compiled with support for Valgrind pmemcheck"
            exit 0
        fi

	valgrind --tool=pmemcheck --help 2>&1 | \
		grep -q "pmemcheck is Copyright (c)" && true
        if [ $? -ne 0 ]; then
            echo "$UNITTEST_NAME: SKIP valgrind package with pmemcheck required"
            exit 0;
        fi

        return
}

#
# require_valgrind_helgrind -- continue script execution only if
#	valgrind with helgrind is installed
#
function require_valgrind_helgrind() {
	require_valgrind
	local binary=$1
	[ -n "$binary" ] || binary=*
        strings ${binary}.static-debug 2>&1 | \
            grep -q "compiled with support for Valgrind helgrind" && true
        if [ $? -ne 0 ]; then
            echo "$UNITTEST_NAME: SKIP not compiled with support for Valgrind helgrind"
            exit 0
        fi

	valgrind --tool=helgrind --help 2>&1 | \
		grep -q "Helgrind is Copyright (C)" && true
        if [ $? -ne 0 ]; then
            echo "$UNITTEST_NAME: SKIP valgrind package with helgrind required"
            exit 0;
        fi

        return
}

#
# require_valgrind_memcheck -- continue script execution only if
#	valgrind with memcheck is installed
#
function require_valgrind_memcheck() {
	require_valgrind
	local binary=$1
	[ -n "$binary" ] || binary=*
	strings ${binary} 2>&1 | \
		grep -q "compiled with support for Valgrind memcheck" && true
	if [ $? -ne 0 ]; then
		echo "$UNITTEST_NAME: SKIP not compiled with support for Valgrind memcheck"
		exit 0
	fi

	return
}

#
# require_valgrind_drd -- continue script execution only if
#	valgrind with drd is installed
#
function require_valgrind_drd() {
	require_valgrind
	local binary=$1
	[ -n "$binary" ] || binary=*
	strings ${binary} 2>&1 | \
		grep -q "compiled with support for Valgrind drd" && true
	if [ $? -ne 0 ]; then
		echo "$UNITTEST_NAME: SKIP not compiled with support for Valgrind drd"
		exit 0
	fi
	return
}

#
# set_valgrind_exe_name -- set the actual Valgrind executable name
#
# On some systems (Ubuntu), "valgrind" is a shell script that calls
# the actual executable "valgrind.bin".
# The wrapper script doesn't work well with LD_PRELOAD, so we want
# to call Valgrind directly.
#
function set_valgrind_exe_name() {
	VALGRINDDIR=`dirname $VALGRINDEXE`
	if [ -x $VALGRINDDIR/valgrind.bin ]; then
		VALGRINDEXE=$VALGRINDDIR/valgrind.bin
	fi
}

#
# require_valgrind_dev_3_7 -- continue script execution only if
#	version 3.7 (or later) of valgrind-devel package is installed
#
function require_valgrind_dev_3_7() {
	require_valgrind
	echo "
        #include <valgrind/valgrind.h>
        #if defined (VALGRIND_RESIZEINPLACE_BLOCK)
        VALGRIND_VERSION_3_7_OR_LATER
        #endif" | gcc ${EXTRA_CFLAGS} -E - 2>&1 | \
		grep -q "VALGRIND_VERSION_3_7_OR_LATER" && return
	echo "$UNITTEST_NAME: SKIP valgrind-devel package (ver 3.7 or later) required"
	exit 0
}

#
# require_valgrind_dev_3_8 -- continue script execution only if
#	version 3.8 (or later) of valgrind-devel package is installed
#
function require_valgrind_dev_3_8() {
	require_valgrind
	echo "
        #include <valgrind/valgrind.h>
        #if defined (__VALGRIND_MAJOR__) && defined (__VALGRIND_MINOR__)
        #if (__VALGRIND_MAJOR__ > 3) || \
             ((__VALGRIND_MAJOR__ == 3) && (__VALGRIND_MINOR__ >= 8))
        VALGRIND_VERSION_3_8_OR_LATER
        #endif
        #endif" | gcc ${EXTRA_CFLAGS} -E - 2>&1 | \
		grep -q "VALGRIND_VERSION_3_8_OR_LATER" && return
	echo "$UNITTEST_NAME: SKIP valgrind-devel package (ver 3.8 or later) required"
	exit 0
}

#
# require_valgrind_dev_3_10 -- continue script execution only if
#	version 3.10 (or later) of valgrind-devel package is installed
#
function require_valgrind_dev_3_10() {
	require_valgrind
	echo "
        #include <valgrind/valgrind.h>
        #if defined (__VALGRIND_MAJOR__) && defined (__VALGRIND_MINOR__)
        #if (__VALGRIND_MAJOR__ > 3) || \
             ((__VALGRIND_MAJOR__ == 3) && (__VALGRIND_MINOR__ >= 10))
        VALGRIND_VERSION_3_10_OR_LATER
        #endif
        #endif" | gcc ${EXTRA_CFLAGS} -E - 2>&1 | \
		grep -q "VALGRIND_VERSION_3_10_OR_LATER" && return
	echo "$UNITTEST_NAME: SKIP valgrind-devel package (ver 3.10 or later) required"
	exit 0
}

#
# require_no_asan_for - continue script execution only if passed binary does
#	NOT require libasan
#
function require_no_asan_for() {
	ASAN_ENABLED=`nm $1 | grep __asan_ | wc -l`
	if [ "$ASAN_ENABLED" != "0" ]; then
		echo "$UNITTEST_NAME: SKIP: ASAN enabled"
		exit 0
	fi
}

#
# require_cxx11 -- continue script execution only if C++11 supporting compiler
#	is installed
#
function require_cxx11() {
	[ "$CXX" ] || CXX=c++

	CXX11_AVAILABLE=`echo "int main(){return 0;}" |\
		$CXX -std=c++11 -x c++ -o /dev/null - 2>/dev/null &&\
		echo y || echo n`

	if [ "$CXX11_AVAILABLE" == "n" ]; then
		echo "$UNITTEST_NAME: SKIP: C++11 required"
		exit 0
	fi
}

#
# require_no_asan - continue script execution only if libpmem does NOT require
#	libasan
#
function require_no_asan() {
	case "$BUILD"
	in
	debug)
		require_no_asan_for ../../debug/libpmem.so
		;;
	nondebug)
		require_no_asan_for ../../nondebug/libpmem.so
		;;
	static-debug)
		require_no_asan_for ../../debug/libpmem.a
		;;
	static-nondebug)
		require_no_asan_for ../../nondebug/libpmem.a
		;;
	esac
}

#
# require_binary -- continue script execution only if the binary has been compiled
#
# In case of conditional compilation, skip this test.
#
function require_binary() {
	if [ -z "$1" ]; then
		echo "require_binary: error: binary not provided" >&2
		exit 1
	fi
	if [ ! -x "$1" ]; then
		echo "$UNITTEST_NAME: SKIP no binary found"
		exit 0
	fi

	return
}

# number of remote nodes required in the current unit test
NODES_MAX=-1

# SSH and SCP options
SSH_OPTS="-o BatchMode=yes"
SCP_OPTS="-o BatchMode=yes -r -p"

# list of common files to be copied to all remote nodes
DIR_SRC="../.."
COMMON_FILES_TO_COPY="\
$DIR_SRC/test/tools/ctrld/ctrld \
$DIR_SRC/tools/pmempool/pmempool"

# array of lists of PID files to be cleaned in case of an error
NODE_PID_FILES[0]=""

#
# run_command -- run a command in a verbose or quiet way
#
function run_command()
{
	local COMMAND="$*"
	if [ "$VERBOSE" != "0" ]; then
		echo "$ $COMMAND"
		$COMMAND
	else
		$COMMAND > /dev/null
	fi
}

#
# validate_node_number -- validate a node number
#
function validate_node_number() {

	[ $1 -gt $NODES_MAX ] \
		&& echo "error: node number ($1) greater than maximum allowed node number ($NODES_MAX)" >&2 \
		&& exit 1
	return 0
}

#
# clean_remote_node -- usage: clean_remote_node <node> <list-of-pid-files>
#
function clean_remote_node() {

	validate_node_number $1

	local N=$1
	shift
	local DIR=${NODE_WORKING_DIR[$N]}/$curtestdir

	# register the list of PID files to be cleaned in case of an error
	NODE_PID_FILES[$N]=$*

	# clean the remote node
	set +e
	for pidfile in ${NODE_PID_FILES[$N]}; do
		run_command ssh $SSH_OPTS ${NODE[$N]} "\
			cd $DIR && [ -f $pidfile ] && \
			./ctrld $pidfile kill SIGINT && \
			./ctrld $pidfile wait 1 ; \
			rm -f $pidfile"
	done;
	set -e

	return 0
}

#
# clean_all_remote_nodes -- clean all remote nodes in case of an error
#
function clean_all_remote_nodes() {

	echo "$UNITTEST_NAME: CLEAN (cleaning processes on remote nodes)"

	local N=0
	set +e
	for (( N=$NODES_MAX ; $(($N + 1)) ; N=$(($N - 1)) )); do
		local DIR=${NODE_WORKING_DIR[$N]}/$curtestdir
		for pidfile in ${NODE_PID_FILES[$N]}; do
			run_command ssh $SSH_OPTS ${NODE[$N]} "\
				cd $DIR && [ -f $pidfile ] && \
				./ctrld $pidfile kill SIGINT && \
				./ctrld $pidfile wait 1 ; \
				rm -f $pidfile"
		done
	done
	set -e

	return 0
}

#
# require_nodes -- only allow script to continue for a certain number
#                  of defined and reachable nodes
#
# Input arguments:
#   NODE[]               - (required) array of nodes' addresses
#   NODE_WORKING_DIR[]   - (required) array of nodes' working directories
#
function require_nodes() {

	# XXX fix it later
	configure_valgrind memcheck force-disable

	local N_NODES=${#NODE[@]}
	local N=$1

	[ $N -gt $N_NODES ] \
		&& echo "$UNITTEST_NAME: SKIP: requires $N node(s), but $N_NODES node(s) provided" \
		&& exit 0

	NODES_MAX=$(($N - 1))

	# check if all required nodes are reachable
	for (( N=$NODES_MAX ; $(($N + 1)) ; N=$(($N - 1)) )); do
		# validate node's address
		[ "${NODE[$N]}" = "" ] \
			&& echo "$UNITTEST_NAME: SKIP: address of node #$N is not provided" \
			&& exit 0

		# validate the working directory
		[ "${NODE_WORKING_DIR[$N]}" = "" ] \
			&& echo "error: working directory for node #$N (${NODE[$N]}) is not provided" >&2 \
			&& exit 1

		# check if the node is reachable
	        set +e
		run_command ssh $SSH_OPTS ${NODE[$N]} exit
		local ret=$?
		set -e
		[ $ret -ne 0 ] \
			&& echo "error: host ${NODE[$N]} is unreachable" >&2 \
			&& exit 1

		# clear the list of PID files for each node
		NODE_PID_FILES[$N]=""
	done

	# files to be copied to all remote nodes
	local FILES_TO_COPY=$COMMON_FILES_TO_COPY

	# add debug or nondebug libraries to the 'to-copy' list
	local BUILD_TYPE=$(echo $BUILD | cut -d"-" -f1)
	[ "$BUILD_TYPE" == "static" ] && BUILD_TYPE=$(echo $BUILD | cut -d"-" -f2)
	FILES_TO_COPY="$FILES_TO_COPY $DIR_SRC/$BUILD_TYPE/*.so.1"

	# copy a binary if it exists
	local TEST_NAME=`echo $UNITTEST_NAME | cut -d"/" -f1`
	local BINARY=$TEST_NAME$EXESUFFIX
	[ -f $BINARY ] && FILES_TO_COPY="$FILES_TO_COPY $BINARY"

	# copy all required files to all required nodes
	for (( N=$NODES_MAX ; $(($N + 1)) ; N=$(($N - 1)) )); do
		# create a new test dir
		local DIR=${NODE_WORKING_DIR[$N]}/$curtestdir
		run_command ssh $SSH_OPTS ${NODE[$N]} "rm -rf $DIR && mkdir -p $DIR"

		# copy all required files
		[ "$FILES_TO_COPY" != "" ] &&\
			run_command scp $SCP_OPTS $FILES_TO_COPY ${NODE[$N]}:$DIR
	done

	# register function to clean all remote nodes in case of an error or SIGINT
	trap clean_all_remote_nodes ERR SIGINT

	return 0
}

#
# copy_files_to_node -- copy all required files to the given remote node
#
function copy_files_to_node() {

	validate_node_number $1

	local N=$1
	shift
	local FILES_TO_COPY=$*
	[ "$FILES_TO_COPY" == "" ] &&\
		echo "error: copy_files_to_node(): no files provided" >&2 && exit 1

	# copy all required files
	local DIR=${NODE_WORKING_DIR[$N]}/$curtestdir
	run_command scp $SCP_OPTS $FILES_TO_COPY ${NODE[$N]}:$DIR

	return 0
}

#
# run_on_node -- usage: run_on_node <node> <command>
#
#                Run the <command> in background on the remote <node>.
#                LD_LIBRARY_PATH for the n-th remote node can be provided
#                in the array NODE_LD_LIBRARY_PATH[n]
#
function run_on_node() {

	validate_node_number $1

	local N=$1
	shift
	local DIR=${NODE_WORKING_DIR[$N]}/$curtestdir
	local COMMAND="UNITTEST_NUM=$UNITTEST_NUM UNITTEST_NAME=$UNITTEST_NAME"
	COMMAND="$COMMAND UNITTEST_QUIET=1"
	COMMAND="$COMMAND LD_LIBRARY_PATH=.:${NODE_LD_LIBRARY_PATH[$N]} $*"

	run_command ssh $SSH_OPTS ${NODE[$N]} "cd $DIR && $COMMAND"
}

#
# run_on_node_background -- usage:
#                           run_on_node_background <node> <pid-file> <command>
#
#                           Run the <command> in background on the remote <node>
#                           and create a <pid-file> for this process.
#                           LD_LIBRARY_PATH for the n-th remote node
#                           can be provided in the array NODE_LD_LIBRARY_PATH[n]
#
function run_on_node_background() {

	validate_node_number $1

	local N=$1
	local PID_FILE=$2
	shift
	shift
	local DIR=${NODE_WORKING_DIR[$N]}/$curtestdir
	local COMMAND="UNITTEST_NUM=$UNITTEST_NUM UNITTEST_NAME=$UNITTEST_NAME"
	COMMAND="$COMMAND UNITTEST_QUIET=1"
	COMMAND="$COMMAND LD_LIBRARY_PATH=.:${NODE_LD_LIBRARY_PATH[$N]}"
	COMMAND="$COMMAND ./ctrld $PID_FILE run $*"

	run_command ssh $SSH_OPTS ${NODE[$N]} "cd $DIR && $COMMAND"
}

#
# wait_on_node -- usage: wait_on_node <node> <pid-file> [<timeout>]
#
#                 Wait until the process with the <pid-file> on the <node>
#                 exits or <timeout> expires.
#
function wait_on_node() {

	validate_node_number $1

	local N=$1
	local PID_FILE=$2
	local TIMEOUT=$3
	local DIR=${NODE_WORKING_DIR[$N]}/$curtestdir

	run_command ssh $SSH_OPTS ${NODE[$N]} "cd $DIR && ./ctrld $PID_FILE wait $TIMEOUT"
}

#
# wait_on_node_port -- usage: wait_on_node_port <node> <pid-file> <portno>
#
#                      Wait until the process with the <pid-file> on the <node>
#                      opens the port <portno>.
#
function wait_on_node_port() {

	validate_node_number $1

	local N=$1
	local PID_FILE=$2
	local PORTNO=$3
	local DIR=${NODE_WORKING_DIR[$N]}/$curtestdir

	run_command ssh $SSH_OPTS ${NODE[$N]} "cd $DIR && ./ctrld $PID_FILE wait_port $PORTNO"
}

#
# kill_on_node -- usage: kill_on_node <node> <pid-file> <signo>
#
#                 Send the <signo> signal to the process with the <pid-file>
#                 on the <node>.
#
function kill_on_node() {

	validate_node_number $1

	local N=$1
	local PID_FILE=$2
	local SIGNO=$3
	local DIR=${NODE_WORKING_DIR[$N]}/$curtestdir

	run_command ssh $SSH_OPTS ${NODE[$N]} "cd $DIR && ./ctrld $PID_FILE kill $SIGNO"
}

#
# setup -- print message that test setup is commencing
#
function setup() {
	# make sure we have a well defined locale for string operations here
	export LC_ALL="C"

	# fs type "none" must be explicitly enabled
	if [ "$FS" = "none" -a "$req_fs_type" != "1" ]; then
		exit 0
	fi

	# fs type "any" must be explicitly enabled
	if [ "$FS" = "any" -a "$req_fs_type" != "1" ]; then
		exit 0
	fi

	if [ "$CHECK_TYPE" != "none" ]; then
		require_valgrind
		export VALGRIND_LOG_FILE=$CHECK_TYPE${UNITTEST_NUM}.log
		MCSTR="/$CHECK_TYPE"
	else
		MCSTR=""
	fi

	echo "$UNITTEST_NAME: SETUP ($TEST/$REAL_FS/$BUILD$MCSTR)"

	rm -f check_pool_${BUILD}_${UNITTEST_NUM}.log

	if [ "$FS" != "none" ]; then
		if [ -d "$DIR" ]; then
			rm --one-file-system -rf -- $DIR
		fi

		mkdir $DIR
	fi
	if [ "$TM" = "1" ]; then
		start_time=$(date +%s.%N)
	fi
}

#
# check -- check test results (using .match files)
#
function check() {
	if [ $NODES_MAX -eq -1 ]; then
		../match $(find . -regex "[^0-9]*${UNITTEST_NUM}\.log\.match" | xargs)
	else
		FILES=$(find . -regex "./node_[0-9]+_[^0-9]*${UNITTEST_NUM}\.log\.match" | xargs)
		for file in $FILES; do
			local N=`echo $file | cut -d"_" -f2`
			local FILE=`echo $file | cut -d"_" -f3 | sed "s/\.match$//g"`
			local DIR=${NODE_WORKING_DIR[$N]}/$curtestdir
			validate_node_number $N
			run_command scp $SCP_OPTS ${NODE[$N]}:$DIR/$FILE .
			run_command mv $FILE ./node\_$N\_$FILE
		done
		../match $(find . -regex "./node_[0-9]+_[^0-9]*${UNITTEST_NUM}\.log\.match" | xargs)
	fi
}

#
# pass -- print message that the test has passed
#
function pass() {
	if [ "$TM" = "1" ]; then
		end_time=$(date +%s.%N)
		tm=$(date -d "0 $end_time sec - $start_time sec" +%H:%M:%S.%N | \
			sed -e "s/^00://g" -e "s/^00://g" -e "s/\([0-9]*\)\.\([0-9][0-9][0-9]\).*/\1.\2/")
		tm="\t\t\t[$tm s]"
	else
		tm=""
	fi
	msg="PASS"
	[ -t 1 ] && command -v tput >/dev/null && msg="$(tput setaf 2)$msg$(tput sgr0)"
	echo -e "$UNITTEST_NAME: $msg$tm"
	if [ "$FS" != "none" ]; then
		rm --one-file-system -rf -- $DIR
	fi
}

# Length of pool file's signature
SIG_LEN=8

# Offset and length of pmemobj layout
LAYOUT_OFFSET=4096
LAYOUT_LEN=1024

# Length of arena's signature
ARENA_SIG_LEN=16

# Signature of BTT Arena
ARENA_SIG="BTT_ARENA_INFO"

# Offset to first arena
ARENA_OFF=8192

#
# check_file -- check if file exists and print error message if not
#
check_file()
{
	if [ ! -f $1 ]
	then
		echo "Missing file: ${1}" >&2
		exit 1
	fi
}

#
# check_files -- check if files exist and print error message if not
#
check_files()
{
	for file in $*
	do
		check_file $file
	done
}

#
# check_no_file -- check if file has been deleted and print error message if not
#
check_no_file()
{
	if [ -f $1 ]
	then
		echo "Not deleted file: ${1}" >&2
		exit 1
	fi
}

#
# check_no_files -- check if files has been deleted and print error message if not
#
check_no_files()
{
	for file in $*
	do
		check_no_file $file
	done
}

#
# get_size -- return size of file
#
get_size()
{
	stat -c%s $1
}

#
# get_mode -- return mode of file
#
get_mode()
{
	stat -c%a $1
}

#
# check_size -- validate file size
#
check_size()
{
	local size=$1
	local file=$2
	local file_size=$(get_size $file)

	if [[ $size != $file_size ]]
	then
		echo "error: wrong size ${file_size} != ${size}" >&2
		exit 1
	fi
}

#
# check_mode -- validate file mode
#
check_mode()
{
	local mode=$1
	local file=$2
	local file_mode=$(get_mode $file)

	if [[ $mode != $file_mode ]]
	then
		echo "error: wrong mode ${file_mode} != ${mode}" >&2
		exit 1
	fi
}

#
# check_signature -- check if file contains specified signature
#
check_signature()
{
	local sig=$1
	local file=$2
	local file_sig=$(dd if=$file bs=1 count=$SIG_LEN 2>/dev/null)

	if [[ $sig != $file_sig ]]
	then
		echo "error: $file: signature doesn't match ${file_sig} != ${sig}" >&2
		exit 1
	fi
}

#
# check_signatures -- check if multiple files contain specified signature
#
check_signatures()
{
	local sig=$1
	shift 1
	for file in $*
	do
		check_signature $sig $file
	done
}

#
# check_layout -- check if pmemobj pool contains specified layout
#
check_layout()
{
	local layout=$1
	local file=$2
	local file_layout=$(dd if=$file bs=1\
		skip=$LAYOUT_OFFSET count=$LAYOUT_LEN 2>/dev/null)

	if [[ $layout != $file_layout ]]
	then
		echo "error: layout doesn't match ${file_layout} != ${layout}" >&2
		exit 1
	fi
}

#
# check_arena -- check if file contains specified arena signature
#
check_arena()
{
	local file=$1
	local sig=$(dd if=$file bs=1 skip=$ARENA_OFF count=$ARENA_SIG_LEN 2>/dev/null)

	if [[ $sig != $ARENA_SIG ]]
	then
		echo "error: can't find arena signature" >&2
		exit 1
	fi
}

#
# dump_pool_info -- dump selected pool metadata and/or user data
#
function dump_pool_info() {
	# ignore selected header fields that differ by definition
	${PMEMPOOL}.static-nondebug info $* | sed -e "/^UUID/,/^Checksum/d"
}

#
# compare_replicas -- check replicas consistency by comparing `pmempool info` output
#
function compare_replicas() {
	set +e
	diff <(dump_pool_info $1 $2) <(dump_pool_info $1 $3)
	set -e
}
