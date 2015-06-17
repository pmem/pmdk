#!/bin/bash
#
# Copyright (c) 2015, Intel Corporation
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
#     * Neither the name of Intel Corporation nor the names of its
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
# check_whitespace.sh -- script for checking trailing whitespace in files
# not processed by cstyle
#

set -e

echo Checking files for trailing spaces...
! find . -path ./src/jemalloc -prune -o\
	-path ./src/debug -prune -o\
	-path ./src/nondebug -prune -o -type f\
	\( -name 'README' -o\
	-name 'README.md' -o\
	-name 'Makefile*' -o\
	-name 'TEST*' -o\
	-name '*.sh' \)\
	-exec grep -n -H -P '\s$$' {} +\
	|| (echo Error: trailing whitespaces found && exit 1)

find doc -name '*.[13]' | while read file
do
	# groff version 1.22 and later has a bug in html generation (via -Thtml)
	# where .nf sections lose their indentation.  This makes the code examples
	# in our man pages all slammed together to the left margin.  Reverting to
	# groff version 1.18.1.4 avoids this bug, so the path to groff is changed
	# to /usr/local/bin for now to remind me to use the local install until
	# the bug is fixed.
	#
	# Even with the old groff, blank lines in the code examples cause the
	# next line to have the incorrect indentation (off by a space).  Adding
	# a tab on the blank line avoids this bug while leaving the output of
	# the man command the same.
	#
	# groff does document that the html driver is just beta quality, so
	# hopefully these issues will be fixed in future updates.
	#
	# The first sed expression ignores all tabs added in blank lines in
	# .nf sections
	sed '/\.nf/,/\.fi/{s/^\t//g}' $file | $(! grep -n -H -P --label=$file '\s$$')
done

echo Done
