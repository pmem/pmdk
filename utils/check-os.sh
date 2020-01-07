#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation

#
# Used to check if there are no banned functions in .o file
#
# usage: ./check-os.sh [os.h path] [.o file] [.c file]

EXCLUDE="os_posix|os_thread_posix"
if [[ $2 =~ $EXCLUDE ]]; then
	echo "skip $2"
	exit 0
fi

symbols=$(nm --demangle --undefined-only --format=posix $2 | sed 's/ U *//g')
functions=$(cat $1 | tr '\n' '|')
functions=${functions%?} # remove trailing | character
out=$(
	for sym in $symbols
	do
		grep -wE $functions <<<"$sym"
	done | sed 's/$/\(\)/g')

[[ ! -z $out ]] &&
	echo -e "`pwd`/$3:1: non wrapped function(s):\n$out\nplease use os wrappers" &&
	rm -f $2 && # remove .o file as it don't match requirements
	exit 1

exit 0
