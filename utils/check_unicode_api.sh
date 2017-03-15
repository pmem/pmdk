#!/bin/bash
#
# Copyright 2017, Intel Corporation
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

#
# Check whether a C header file declares/defines functions which use
# C-strings and verifies that Windows specific U/W functions are present.
#
# usage: ./check_unicode_api.sh [-i inc_dir] [-x exclude_pattern] <header_files>
#

borked=0

function usage {
	echo "Usage: $0 [-i inc_dir] [-x exclude_pattern] <header_files>"
	exit 1;
}

function check_file {
	local file=$1
	local dir=$2
	local pat=$3

	local funcs=$(clang -Xclang -ast-dump $file -fno-color-diagnostics |\
		grep "FunctionDecl.*pmem.*char \*" | cut -d " " -f 6)
	for func in $funcs
	do
		local good=0
		to_check="$dir/*.h $file"
		if [ -n "${pat:+x}" ] && [[ $func =~ $pat ]]; then
			continue
		fi

		for f in $to_check
		do
			let good+=$(grep -c "$func[UW][ ]*(" $f)
		done

		if [ $good -ne 2 ]; then
			echo "Function $func in file $file does not have unicode U/W counterparts"
			borked=1;
		fi
	done
}

#
# defaul values
#
inc_dir=""
exc_patt=""

#
# command-line argument processing...
#
args=`getopt i:x: $*`
[ $? != 0 ] && usage
set -- $args
for arg
do
	case "$arg"
	in
	-i)
		inc_dir="$2"
		shift 2
		;;
	-x)
		exc_patt="$2"
		shift 2
		;;
	--)
		shift
		break
		;;
	esac
done

for f in $*
do
	check_file $f $inc_dir $exc_patt
done

if [ $borked -ne 0 ]; then
	exit 1
fi
