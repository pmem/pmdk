#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation
#
# utils/style_check.sh -- common style checking script
#
set -e

ARGS=("$@")
CSTYLE_ARGS=()
CLANG_ARGS=()
FLAKE8_ARGS=()
CHECK_TYPE=$1

[ -z "$clang_format_bin" ] && which clang-format-9 >/dev/null &&
	clang_format_bin=clang-format-9
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
# require clang-format version 9.0
#
function check_clang_version() {
	set +e
	which ${clang_format_bin} &> /dev/null && ${clang_format_bin} --version |\
	grep "version 9\.0"\
	&> /dev/null
	if [ $? -ne 0 ]; then
		echo "SKIP: requires clang-format version 9.0"
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

function run_flake8() {
	if [ $# -eq 0 ]; then
		return
	fi
	${flake8_bin} --exclude=testconfig.py $@
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

		*.py)
			FLAKE8_ARGS+="${ARGS[$i]} "
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
		run_flake8 ${FLAKE8_ARGS}
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
