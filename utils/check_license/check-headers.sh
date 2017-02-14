#!/bin/bash
#
# Copyright 2016-2017, Intel Corporation
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

SELF=$0

function usage() {
	echo "Usage: $SELF <source_root_path> <check_license_bin_path> <license_path> [-h|-v|-a]"
	echo "   -h, --help      this help message"
	echo "   -v, --verbose   verbose mode"
	echo "   -a, --all       check all files (only modified files are checked by default)"
}

if [ "$#" -lt 3 ]; then
	usage >&2
	exit 2
fi

SOURCE_ROOT=$1
shift
CHECK_LICENSE=$1
shift
LICENSE=$1
shift

PATTERN=`mktemp`
TMP=`mktemp`
rm -f $PATTERN $TMP

function exit_if_not_exist()
{
	if [ ! -f $1 ]; then
		echo "Error: file $1 does not exist. Exiting..." >&2
		exit 1
	fi
}

if [ "$1" == "-h" -o "$1" == "--help" ]; then
	usage
	exit 0
fi

exit_if_not_exist $LICENSE
exit_if_not_exist $CHECK_LICENSE

git rev-parse || exit 1

if [ -f $SOURCE_ROOT/.git/shallow ]; then
	SHALLOW_CLONE=1
	echo
	echo "Warning: This is a shallow clone. Checking dates in copyright headers"
	echo "         will be skipped in case of files that have no history."
	echo
else
	SHALLOW_CLONE=0
fi

VERBOSE=0
CHECK_ALL=0
while [ "$1" != "" ]; do
	case $1 in
	-v|--verbose)
		VERBOSE=1
		;;
	-a|--all)
		CHECK_ALL=1
		;;
	esac
	shift
done

if [ $CHECK_ALL -eq 0 ]; then
	CURRENT_COMMIT=$(git log --pretty=%H -1)
	MERGE_BASE=$(git merge-base HEAD origin/master 2>/dev/null)
	[ -z $MERGE_BASE ] && \
		MERGE_BASE=$(git log --pretty="%cN:%H" | grep GitHub | head -n1 | cut -d: -f2)
	[ -z $MERGE_BASE -o "$CURRENT_COMMIT" = "$MERGE_BASE" ] && \
		CHECK_ALL=1
fi

if [ $CHECK_ALL -eq 1 ]; then
	echo "Checking copyright headers of all files..."
	GIT_COMMAND="ls-tree -r --name-only HEAD $SOURCE_ROOT"
else
	echo
	echo "Warning: will check copyright headers of modified files only,"
	echo "         in order to check all files issue the following command:"
	echo "         $ $SELF <source_root_path> <check_license_bin_path> <license_path> -a"
	echo "         (e.g.: $ $SELF $SOURCE_ROOT $CHECK_LICENSE $LICENSE -a)"
	echo
	echo "Checking copyright headers of modified files only..."
	GIT_COMMAND="diff --name-only $MERGE_BASE $CURRENT_COMMIT $SOURCE_ROOT"
fi

FILES=$(git $GIT_COMMAND | \
	grep -v -E -e 'src/jemalloc/' -e 'src/windows/jemalloc_gen/' -e '/queue.h$' -e '/ListEntry.h$' \
		   -e '/getopt.h$' -e '/getopt.c$' | \
	grep    -E -e '*\.[chs]$' -e '*\.[ch]pp$' -e '*\.sh$' \
		   -e '*\.py$' -e '*\.map$' -e 'Makefile*' -e 'TEST*' \
		   -e '/common.inc$' -e '/match$' -e '/check_whitespace$' \
		   -e 'LICENSE$' | \
	xargs)

# jemalloc.mk has to be checked always, because of the grep rules above
FILES="$FILES $SOURCE_ROOT/src/jemalloc/jemalloc.mk"

# create a license pattern file
$CHECK_LICENSE create $LICENSE $PATTERN
[ $? -ne 0 ] && exit 1

RV=0
for file in $FILES ; do
	[ ! -f $file ] && continue
	YEARS=`$CHECK_LICENSE check-pattern $PATTERN $file`
	if [ $? -ne 0 ]; then
		echo -n $YEARS
		RV=1
	else
		HEADER_FIRST=`echo $YEARS | cut -d"-" -f1`
		HEADER_LAST=` echo $YEARS | cut -d"-" -f2`
		git log --no-merges --format="%ai %H %aE" -- $file | sort > $TMP
		FIRST=`cat $TMP | head -n1`
		LAST=` cat $TMP | tail -n1`

		# skip checking dates for non-Intel commits
		AUTHOR_LAST=`echo $LAST | cut -d"@" -f2`
		[ "AUTHOR_LAST" != "intel.com" ] && continue

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
				echo "error: wrong copyright date in file: $file (is: $YEARS, should be: $NEW)" >&2
				RV=1
			fi
		else
			echo "error: unknown commit dates in file: $file" >&2
			RV=1
		fi
	fi
done
rm -f $TMP

# check if error found
if [ $RV -eq 0 ]; then
	echo "Copyright headers are OK."
else
	echo "Error(s) in copyright headers found!" >&2
fi
exit $RV
