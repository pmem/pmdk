#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2022, Intel Corporation

#
# src2mans -- extract man pages from source files
#

DIR=$1
MAN_3=$2
MAN_7=$3
MANS_HEADER=$4
[ "$5" == "fix" ] && FIX=1 || FIX=0

if [ $# -lt 4 ] || [ ! -d $DIR ] || [ ! -f $MAN_3 ] || [ ! -f $MAN_7 ] || [ ! -f $MANS_HEADER ]; then
	echo "$ $0 $*"
	echo "Error: missing or wrong argument"
	echo
	echo "Usage: $(basename $0) <directory> <man3-file> <man7-file> <mans_header> [fix]"
	echo "   <directory>   - directory to be searched for *.h files"
	echo "   <man3-file>   - file containing list of section #3 manuals"
	echo "   <man7-file>   - file containing list of section #7 manuals"
	echo "   <mans_header> - common header of markup manuals"
	echo "   fix           - fix files containing list of manuals"
	echo
	[ ! -d $DIR ] && echo "Error: $DIR does not exist or is not a directory"
	[ ! -f $MAN_3 ] && echo "Error: $MAN_3 does not exist or is not a regular file"
	[ ! -f $MAN_7 ] && echo "Error: $MAN_7 does not exist or is not a regular file"
	[ ! -f $MANS_HEADER ] && echo "Error: $MANS_HEADER does not exist or is not a regular file"
	exit 1
fi

function check_manuals_list() {
	N=$1
	LIST=$2
	CURRENT=$3
	FIX=$4
	if ! diff $LIST $CURRENT; then
		if [ $FIX -eq 1 ]; then
			mv $CURRENT $LIST
			echo "Updated the file: $LIST"
		else
			echo "Error: current list of manuals($N) does match the file: $LIST"
			RV=1
		fi
	fi
}

PANDOC=0
if which pandoc > /dev/null; then
	PANDOC=1
	mkdir -p md
else
	echo "Warning: pandoc not found, Markdown documentation will not be generated" >&2
fi

ALL_MANUALS="$(mktemp)"

find $DIR -name '*.h' -print0 | while read -d $'\0' MAN
do
	MANUALS="$(mktemp)"
	ERRORS="$(mktemp)"

	src2man -r MINIASYNC -v "MINIASYNC Programmer's Manual" $MAN > $MANUALS 2> $ERRORS
	# gawk 5.0.1 does not recognize expressions \;|\,|\o  as regex operator
	sed -i -r "/warning: regexp escape sequence \`[\][;,o]' is not a known regexp operator/d" $ERRORS
	# remove empty lines
	sed -i '/^$/d' $ERRORS

	if [[ -s "$ERRORS" ]]; then
		echo "src2man: errors found in the \"$MAN\" file:"
		cat $ERRORS
		exit 1
	fi

	if [ $PANDOC -eq 1 ]; then
		for f in $(cat $MANUALS | xargs); do
			# get rid of a FILE section (last two lines of the file)
			mv $f $f.tmp
			head -n -2 $f.tmp > $f
			rm $f.tmp

			# generate a md file
			pandoc -s $f -o $f.tmp1 -f man -t markdown || break
			# remove the header
			tail -n +6 $f.tmp1 > $f.tmp2
			# fix the name issue '**a **-' -> '**a** -'
			sed -i '5s/ \*\*-/\*\* -/' $f.tmp2
			# start with a custom header
			cat $MANS_HEADER > md/$f.md
			cat $f.tmp2 >> md/$f.md
			rm $f.tmp1 $f.tmp2
		done
	fi

	# save all manuals
	cat $MANUALS >> $ALL_MANUALS

	rm $MANUALS $ERRORS
done || exit 1

NEW_MAN_3="$(mktemp)"
NEW_MAN_7="$(mktemp)"
cat $ALL_MANUALS | grep -e '\.3' | sort > $NEW_MAN_3
cat $ALL_MANUALS | grep -e '\.7' | sort > $NEW_MAN_7

# check if all generated manuals are listed in the manuals' files
RV=0
check_manuals_list 3 $MAN_3 $NEW_MAN_3 $FIX
check_manuals_list 7 $MAN_7 $NEW_MAN_7 $FIX
if [ $RV -eq 1 -a $FIX -eq 0 ]; then
	echo "In order to fix it, run 'make doc-fix'"
	echo
fi

rm -f $ALL_MANUALS $NEW_MAN_3 $NEW_MAN_7
exit $RV
