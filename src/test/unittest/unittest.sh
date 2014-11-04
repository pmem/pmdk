#
# Copyright (c) 2014, Intel Corporation
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
#     * Neither the name of Intel Corporation nor the names of its
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
[ "$FS" ] || export FS=local
[ "$BUILD" ] || export BUILD=debug

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
# The variable DIR is constructed so the test uss that directory when
# constructing test files.  DIR is chosen based on the fs-type for
# this test, and if the appropriate fs-type doesn't have a directory
# defined in testconfig.sh, the test is skipped.
#
# This behavior can be overridden by setting DIR.  For example:
#	DIR=/force/test/dir ./TEST0
#
[ "$DIR" ] || {
	case "$FS"
	in
	local)
		DIR=$LOCAL_FS_DIR
		;;
	pmem)
		DIR=$PMEM_FS_DIR
		;;
	non-pmem)
		DIR=$NON_PMEM_FS_DIR
		;;
	esac
	[ "$DIR" ] || {
		[ "$UNITTEST_QUIET" ] || echo "$UNITTEST_NAME: SKIP fs-type $FS (not configured)"
		exit 0
	}
}

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
# expect_normal_exit -- run a given command, expect it to exit 0
#
function expect_normal_exit() {
	eval $ECHO LD_LIBRARY_PATH=$TEST_LD_LIBRARY_PATH \
	$TRACE $*
}

#
# expect_abnormal_exit -- run a given command, expect it to exit non-zero
#
function expect_abnormal_exit() {
	set +e
	eval $ECHO LD_LIBRARY_PATH=$TEST_LD_LIBRARY_PATH \
	$TRACE $*
	set -e
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
# require_fs_type -- only allow script to continue for a certain fs type
#
function require_fs_type() {
	for type in $*
	do
		[ "$type" = "$FS" ] && return
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
# setup -- print message that test setup is commencing
#
function setup() {
	echo "$UNITTEST_NAME: SETUP ($TEST/$FS/$BUILD)"
}

#
# check -- check test results (using .match files)
#
function check() {
	../match $(find . -regex "[^0-9]*${UNITTEST_NUM}\.log\.match" | xargs)
}

#
# pass -- print message that the test has passed
#
function pass() {
	echo $UNITTEST_NAME: PASS
}
