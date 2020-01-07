#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation

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
TMP2=`mktemp`
TEMPFILE=`mktemp`
rm -f $PATTERN $TMP $TMP2

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

export GIT="git -C ${SOURCE_ROOT}"
$GIT rev-parse || exit 1

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
	CURRENT_COMMIT=$($GIT log --pretty=%H -1)
	MERGE_BASE=$($GIT merge-base HEAD origin/master 2>/dev/null)
	[ -z $MERGE_BASE ] && \
		MERGE_BASE=$($GIT log --pretty="%cN:%H" | grep GitHub | head -n1 | cut -d: -f2)
	[ -z $MERGE_BASE -o "$CURRENT_COMMIT" = "$MERGE_BASE" ] && \
		CHECK_ALL=1
fi

if [ $CHECK_ALL -eq 1 ]; then
	echo "Checking copyright headers of all files..."
	GIT_COMMAND="ls-tree -r --name-only HEAD"
else
	if [ $VERBOSE -eq 1 ]; then
		echo
		echo "Warning: will check copyright headers of modified files only,"
		echo "         in order to check all files issue the following command:"
		echo "         $ $SELF <source_root_path> <check_license_bin_path> <license_path> -a"
		echo "         (e.g.: $ $SELF $SOURCE_ROOT $CHECK_LICENSE $LICENSE -a)"
		echo
	fi
	echo "Checking copyright headers of modified files only..."
	GIT_COMMAND="diff --name-only $MERGE_BASE $CURRENT_COMMIT"
fi

FILES=$($GIT $GIT_COMMAND | ${SOURCE_ROOT}/utils/check_license/file-exceptions.sh | \
	grep    -E -e '*\.[chs]$' -e '*\.[ch]pp$' -e '*\.sh$' \
		   -e '*\.py$' -e '*\.link$' -e 'Makefile*' -e 'TEST*' \
		   -e '/common.inc$' -e '/match$' -e '/check_whitespace$' \
		   -e 'LICENSE$' -e 'CMakeLists.txt$' -e '*\.cmake$' | \
	xargs)

# create a license pattern file
$CHECK_LICENSE create $LICENSE $PATTERN
[ $? -ne 0 ] && exit 1

RV=0
for file in $FILES ; do
	# The src_path is a path which should be used in every command except git.
	# git is called with -C flag so filepaths should be relative to SOURCE_ROOT
	src_path="${SOURCE_ROOT}/$file"
	[ ! -f $src_path ] && continue
	# ensure that file is UTF-8 encoded
	ENCODING=`file -b --mime-encoding $src_path`
	iconv -f $ENCODING -t "UTF-8" $src_path > $TEMPFILE

	YEARS=`$CHECK_LICENSE check-pattern $PATTERN $TEMPFILE $src_path`
	if [ $? -ne 0 ]; then
		echo -n $YEARS
		RV=1
	else
		HEADER_FIRST=`echo $YEARS | cut -d"-" -f1`
		HEADER_LAST=` echo $YEARS | cut -d"-" -f2`

		if [ $SHALLOW_CLONE -eq 0 ]; then
			$GIT log --no-merges --format="%ai %aE" -- $file | sort > $TMP
		else
			# mark the grafted commits (commits with no parents)
			$GIT log --no-merges --format="%ai %aE grafted-%p-commit" -- $file | sort > $TMP
		fi

		# skip checking dates for non-Intel commits
		[[ ! $(tail -n1 $TMP) =~ "@intel.com" ]] && continue

		# skip checking dates for new files
		[ $(cat $TMP | wc -l) -le 1 ] && continue

		# grep out the grafted commits (commits with no parents)
		# and skip checking dates for non-Intel commits
		grep -v -e "grafted--commit" $TMP | grep -e "@intel.com" > $TMP2

		[ $(cat $TMP2 | wc -l) -eq 0 ] && continue

		FIRST=`head -n1 $TMP2`
		LAST=` tail -n1 $TMP2`

		COMMIT_FIRST=`echo $FIRST | cut -d"-" -f1`
		COMMIT_LAST=` echo $LAST  | cut -d"-" -f1`
		if [ "$COMMIT_FIRST" != "" -a "$COMMIT_LAST" != "" ]; then
			if [ $HEADER_LAST -lt $COMMIT_LAST ]; then
				if [ $HEADER_FIRST -lt $COMMIT_FIRST ]; then
					COMMIT_FIRST=$HEADER_FIRST
				fi
				COMMIT_LAST=`date +%G`
				if [ $COMMIT_FIRST -eq $COMMIT_LAST ]; then
					NEW=$COMMIT_LAST
				else
					NEW=$COMMIT_FIRST-$COMMIT_LAST
				fi
				echo "$file:1: error: wrong copyright date: (is: $YEARS, should be: $NEW)" >&2
				RV=1
			fi
		else
			echo "error: unknown commit dates in file: $file" >&2
			RV=1
		fi
	fi
done
rm -f $TMP $TMP2 $TEMPFILE

# check if error found
if [ $RV -eq 0 ]; then
	echo "Copyright headers are OK."
else
	echo "Error(s) in copyright headers found!" >&2
fi
exit $RV
