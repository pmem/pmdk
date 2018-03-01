#!/usr/bin/env bash
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

function usage() {
	fatal "invalid getopt configuration"
}

args=`getopt t:f:b:m:p:e:d:sc "$@"`

[ $? != 0 ] && usage

eval set -- $args

while true; do
	case "$1" in
		-t)
			TEST_req_test_type=$2
			shift 2
			;;
		-f)
			TEST_req_fs_type=$2
			shift 2
			;;
		-b)
			TEST_req_build_type=$2
			shift 2
			;;
		-m)
			TEST_memcheck=$2
			shift 2
			;;
		-p)
			TEST_pmemcheck=$2
			shift 2
			;;
		-e)
			TEST_helgrind=$2
			shift 2
			;;
		-d)
			TEST_drd=$2
			shift 2
			;;
		-s)
			TEST_run_setup=1
			shift
			;;
		-c)
			TEST_run_check=1
			shift
			;;
		--)
			# end of options
			shift
			break
			;;
		*)
			echo $1
			echo $2
			fatal "getopt error"
			;;
	esac
done

export SCRIPTNAME=$(basename $1)

source ../unittest/unittest.sh

if [ -n "${TEST_req_test_type}" ]; then
	require_test_type ${TEST_req_test_type}
fi

if [ -n "${TEST_req_fs_type}" ]; then
	require_fs_type ${TEST_req_fs_type}
fi

if [ -n "${TEST_req_build_type}" ]; then
	require_build_type ${TEST_req_build_type}
fi

if [ -n "${TEST_memcheck}" ]; then
	configure_valgrind memcheck ${TEST_memcheck}
fi

if [ -n "${TEST_pmemcheck}" ]; then
	configure_valgrind pmemcheck ${TEST_pmemcheck}
fi

if [ -n "${TEST_helgrind}" ]; then
	configure_valgrind helgrind ${TEST_helgrind}
fi

if [ -n "${TEST_drd}" ]; then
	configure_valgrind drd ${TEST_drd}
fi

if [ -n "${TEST_run_setup}" ]; then
	setup
fi

source $1

if [ -n "${TEST_run_check}" ]; then
	check
fi

pass
