#!/usr/bin/env bash
#
# Copyright 2016-2019, Intel Corporation
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
title=`sed -n 's/^title:\ _MP(*\([A-Za-z_-]*\).*$/\1/p' $filename`
section=`sed -n 's/^title:.*\([0-9]\))$/\1/p' $filename`
version=`sed -n 's/^date:\ *\(.*\)$/\1/p' $filename`

if [ "$TESTOPTS" != "" ]; then
	m4 $TESTOPTS macros.man $filename | sed -n -e '/# NAME #/,$p' > $outfile
else
	OPTS=

if [ "$WIN32" == 1 ]; then
	OPTS="$OPTS -DWIN32"
else
	OPTS="$OPTS -UWIN32"
fi

if [ "$(uname -s)" == "FreeBSD" ]; then
	OPTS="$OPTS -DFREEBSD"
else
	OPTS="$OPTS -UFREEBSD"
fi

if [ "$WEB" == 1 ]; then
	OPTS="$OPTS -DWEB"
	mkdir -p "$(dirname $outfile)"
	m4 $OPTS macros.man $filename | sed -n -e '/---/,$p' > $outfile
else
	SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(date +%s)}"
	YEAR=$(date -u -d "@$SOURCE_DATE_EPOCH" +%Y 2>/dev/null ||
		date -u -r "$SOURCE_DATE_EPOCH" +%Y 2>/dev/null || date -u +%Y)
	dt=$(date -u -d "@$SOURCE_DATE_EPOCH" +%F 2>/dev/null ||
		date -u -r "$SOURCE_DATE_EPOCH" +%F 2>/dev/null || date -u +%F)
	m4 $OPTS macros.man $filename | sed -n -e '/# NAME #/,$p' |\
		pandoc -s -t man -o $outfile.tmp --template=$template \
		-V title=$title -V section=$section \
		-V date="$dt" -V version="$version" \
		-V year="$YEAR" |
sed '/^\.IP/{
N
/\n\.nf/{
	s/IP/PP/
    }
}'

	# don't overwrite the output file if the only thing that changed
	# is modification date (diff output has exactly 4 lines in this case)
	difflines=`diff $outfile $outfile.tmp | wc -l || true`
	onlydates=`diff $outfile $outfile.tmp | grep "$dt" | wc -l || true`
	if [ $difflines -eq 4 -a $onlydates -eq 1 ]; then
		rm $outfile.tmp
	else
		mv $outfile.tmp $outfile
	fi

fi
fi
