#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation
#

#
# md2man.sh -- convert markdown to groff man pages
#
# usage: md2man.sh file template outfile
#
# This script converts markdown file into groff man page using pandoc.
# It performs some pre- and post-processing for better results:
# - uses m4 to preprocess OS-specific directives. See doc/macros.man.
# - parse input file for YAML metadata block and read man page title,
#   section and version
# - cut-off metadata block and license
# - unindent code blocks
# - cut-off windows and web specific parts of documentation
#
# If the TESTOPTS variable is set, generates a preprocessed markdown file
# with the header stripped off for testing purposes.
#

set -e
set -o pipefail

filename=$1
template=$2
outfile=$3
title=`sed -n 's/^title:\ *\([A-Za-z0-9_-]*\).*$/\1/p' $filename`
section=`sed -n 's/^title:.*\([0-9]\))$/\1/p' $filename`
version=`sed -n 's/^date:\ *\(.*\)$/\1/p' $filename`

if [ "$TESTOPTS" != "" ]; then
	cat $filename | sed -n -e '/# NAME #/,$p' > $outfile
else
	OPTS=

if [ "$WEB" == 1 ]; then
	OPTS="$OPTS -DWEB"
	mkdir -p "$(dirname $outfile)"
	cat $filename | sed -n -e '/---/,$p' > $outfile
else
	SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(date +%s)}"
	COPYRIGHT=$(grep -rwI "\[comment]: <> (Copyright" $filename |\
		sed "s/\[comment\]: <> (\([^)]*\))/\1/")
	dt=$(date -u -d "@$SOURCE_DATE_EPOCH" +%F 2>/dev/null ||
		date -u -r "$SOURCE_DATE_EPOCH" +%F 2>/dev/null || date -u +%F)
	cat $filename | sed -n -e '/# NAME #/,$p' |\
		pandoc -s -t man -o $outfile --template=$template \
		-V title=$title -V section=$section \
		-V date="$dt" -V version="$version" \
		-V copyright="$COPYRIGHT"
fi
fi
