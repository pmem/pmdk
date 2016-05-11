#!/bin/bash
#
# Copyright 2016, Intel Corporation
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

# check-headers.sh - check copyright and license in source files

SH_FILE="utils\/check_license\/check-headers\.sh"
BIN_FILE="utils\/check_license\/check-license"
LIC_FILE="LICENSE"

PWD=`pwd -P`
SELF_FULL=`echo "$PWD/$0" | sed "s/\/\.\//\//g" `
NVML=`echo $SELF_FULL | sed "s/$SH_FILE//g" `
LICENSE=`echo $SELF_FULL | sed "s/$SH_FILE/$LIC_FILE/g" `
CHECK_LICENSE=`echo $SELF_FULL | sed "s/$SH_FILE/$BIN_FILE/g" `

PATTERN=`mktemp`
TMP=`mktemp`
rm -f $PATTERN $TMP

function exit_if_not_exist()
{
	if [ ! -f $1 ]; then
		echo "Error: file $1 does not exist. Exiting..."
		exit 1
	fi
}

if [ "$1" == "-h" -o "$1" == "--help" ]; then
	echo "Usage: $0 [-h|-v]"
	echo "   -h, --help      this help message"
	echo "   -v, --verbose   verbose mode"
	exit 0
fi

[ "$1" == "-v" -o "$1" == "--verbose" ] && VERBOSE=1 || VERBOSE=0

echo "Checking copyright headers..."

exit_if_not_exist $LICENSE
exit_if_not_exist $CHECK_LICENSE

git rev-parse || exit 1

if [ -f $NVML/.git/shallow ]; then
	SHALLOW_CLONE=1
	echo "Warning: This is a shallow clone. Checking dates in copyright headers"
	echo "         will be skipped in case of files that have no history."
else
	SHALLOW_CLONE=0
fi

FILES=`git ls-tree -r --name-only HEAD | \
	grep -v -E -e 'jemalloc' -e 'queue.h' -e 'ListEntry.h' | \
	grep    -E -e '*\.[ch]$' -e '*\.[ch]pp$' -e '*\.[12345]$' -e '*\.sh$' \
		   -e '*\.py$' -e '*\.map$' -e 'Makefile*' -e 'TEST*' | \
	xargs`
FILES="$FILES $NVML/src/common.inc $NVML/src/jemalloc/jemalloc.mk \
	$NVML/src/test/match $NVML/utils/check_whitespace $NVML/LICENSE"

# create a license pattern file
$CHECK_LICENSE create $LICENSE $PATTERN
[ $? -ne 0 ] && exit 1

RV=0
for file in $FILES ; do
	YEARS=`$CHECK_LICENSE check-pattern $PATTERN $file`
	if [ $? -ne 0 ]; then
		echo -n $YEARS
		RV=1
	else
		HEADER_FIRST=`echo $YEARS | cut -d"-" -f1`
		HEADER_LAST=` echo $YEARS | cut -d"-" -f2`
		git log --no-merges --format="%ai %H" -- $file | sort > $TMP
		FIRST=`cat $TMP | head -n1`
		LAST=` cat $TMP | tail -n1`
		COMMIT_FIRST=`echo $FIRST | cut -d"-" -f1`
		COMMIT_LAST=` echo $LAST  | cut -d"-" -f1`
		SKIP=0
		if [ $SHALLOW_CLONE -eq 1 ]; then
			HASH_FIRST=`echo $FIRST | cut -d" " -f4`
			HASH_LAST=` echo $LAST  | cut -d" " -f4`
			if [ "$HASH_FIRST" == "$HASH_LAST" ]; then
				CHANGED=`git diff --name-only $HASH_FIRST -- $file`
				if [ "$CHANGED" == "" ]; then
					SKIP=1
					[ $VERBOSE -eq 1 ] && echo "info: checking dates in file '$file' skipped (no history)"
				fi
			fi
		fi
		if [ "$COMMIT_FIRST" != "" -a "$COMMIT_LAST" != "" ]; then
			if [ $SKIP -eq 0 -a $HEADER_LAST -lt $COMMIT_LAST ]; then
				if [ $HEADER_FIRST -lt $COMMIT_FIRST ]; then
					COMMIT_FIRST=$HEADER_FIRST
				fi
				COMMIT_LAST=`date +%G`
				if [ $COMMIT_FIRST -eq $COMMIT_LAST ]; then
					NEW=$COMMIT_LAST
				else
					NEW=$COMMIT_FIRST-$COMMIT_LAST
				fi
				echo "error: wrong copyright date in file: $file (is: $YEARS, should be: $NEW)"
				RV=1
			fi
		else
			echo "error: unknown commit dates in file: $file"
			RV=1
		fi
	fi
done
rm -f $TMP

# check if error found
if [ $RV -eq 0 ]; then
	echo "Copyright headers are OK."
else
	echo "Error(s) in copyright headers found!"
fi
exit $RV
