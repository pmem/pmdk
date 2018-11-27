#!/usr/bin/env bash
#
# Copyright 2016-2018, Intel Corporation
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
# utils/style_check.sh -- common style checking script
#
set -e

ARGS=("$@")
CSTYLE_ARGS=()
CLANG_ARGS=()
CHECK_TYPE=$1

[ -z "$clang_format_bin" ] && which clang-format-6.0 >/dev/null &&
	clang_format_bin=clang-format-6.0
[ -z "$clang_format_bin" ] && which clang-format >/dev/null &&
	clang_format_bin=clang-format
[ -z "$clang_format_bin" ] && clang_format_bin=clang-format

#
# print script usage
#
function usage() {
	echo "$0 <check|format> [C/C++ files]"
}

#
# require clang-format version 6.0
#
function check_clang_version() {
	set +e
	which ${clang_format_bin} &> /dev/null && ${clang_format_bin} --version |\
	grep "version 6\.0"\
	&> /dev/null
	if [ $? -ne 0 ]; then
		echo "SKIP: requires clang-format version 6.0"
		exit 0
	fi
	set -e
}

#
# run old cstyle check
#
function run_cstyle() {
	if [ $# -eq 0 ]; then
		return
	fi

	${cstyle_bin} -pP $@
}

#
# generate diff with clang-format rules
#
function run_clang_check() {
	if [ $# -eq 0 ]; then
		return
	fi
	check_clang_version

	for file in $@
	do
		LINES=$(${clang_format_bin} -style=file $file |\
				git diff --no-index $file - | wc -l)
		if [ $LINES -ne 0 ]; then
			${clang_format_bin} -style=file $file | git diff --no-index $file -
		fi
	done
}

#
# in-place format according to clang-format rules
#
function run_clang_format() {
	if [ $# -eq 0 ]; then
		return
	fi
	check_clang_version

	${clang_format_bin} -style=file -i $@
}

for ((i=1; i<$#; i++)) {

	IGNORE="$(dirname ${ARGS[$i]})/.cstyleignore"
	if [ -e $IGNORE ]; then
		if grep -q ${ARGS[$i]} $IGNORE ; then
			echo "SKIP ${ARGS[$i]}"
			continue
		fi
	fi
	case ${ARGS[$i]} in
		*.[ch]pp)
			CLANG_ARGS+="${ARGS[$i]} "
			;;

		*.[ch])
			CSTYLE_ARGS+="${ARGS[$i]} "
			;;

		*)
			echo "Unknown argument"
			exit 1
			;;
	esac
}

case $CHECK_TYPE in
	check)
		run_cstyle ${CSTYLE_ARGS}
		run_clang_check ${CLANG_ARGS}
		;;

	format)
		run_clang_format ${CLANG_ARGS}
		;;

	*)
		echo "Invalid parameters"
		usage
		exit 1
		;;
esac
