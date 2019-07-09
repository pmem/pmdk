#!/usr/bin/env bash
#
# Copyright 2018-2019, Intel Corporation
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
# Finds applicable area name for specified commit id.
#

if [ -z "$1" ]; then
	echo "Missing commit id argument."
	exit 1
fi

files=`git show $1 | grep -e "^--- a/" -e "^+++ b/" | grep -v /dev/null | sed "s/^--- a\///" | sed "s/^+++ b\///" | uniq`

git show -q $1 | cat

echo
echo "Modified files:"
echo "$files"

function categorize() {
	category=$1
	shift
	cat_files=`echo "$files" | grep $*`

	if [ -n "${cat_files}" ]; then
		echo "$category"
		files=`echo "$files" | grep -v $*`
	fi
}

echo
echo "Areas computed basing on the list of modified files: (see utils/check-area.sh for full algorithm)"

categorize pmem      -e "^src/libpmem/"     -e "^src/include/libpmem.h"
categorize rpmem     -e "^src/librpmem/"    -e "^src/include/librpmem.h" -e "^src/tools/rpmemd/" -e "^src/rpmem_common/"
categorize log       -e "^src/libpmemlog/"  -e "^src/include/libpmemlog.h"
categorize blk       -e "^src/libpmemblk/"  -e "^src/include/libpmemblk.h"
categorize obj       -e "^src/libpmemobj/"  -e "^src/include/libpmemobj.h" -e "^src/include/libpmemobj/"
categorize pool      -e "^src/libpmempool/" -e "^src/include/libpmempool.h" -e "^src/tools/pmempool/"
categorize vmem      -e "^src/libvmem/"     -e "^src/include/libvmem.h"
categorize vmmalloc  -e "^src/libvmmalloc/" -e "^src/include/libvmmalloc.h"
categorize jemalloc  -e "^src/jemalloc/"    -e "^src/windows/jemalloc_gen/"
categorize benchmark -e "^src/benchmarks/"
categorize examples  -e "^src/examples/"
categorize daxio     -e "^src/tools/daxio/"
categorize pmreorder -e "^src/tools/pmreorder/"
categorize test      -e "^src/test/"
categorize doc       -e "^doc/" -e ".md\$" -e "^ChangeLog" -e "README"
categorize common    -e "^src/common/" \
			-e "^utils/" \
			-e ".inc\$" \
			-e ".yml\$" \
			-e ".gitattributes" \
			-e ".gitignore" \
			-e "^.mailmap\$" \
			-e "^src/PMDK.sln\$" \
			-e "Makefile\$" \
			-e "^src/freebsd/" \
			-e "^src/windows/" \
			-e "^src/include/pmemcompat.h"

echo
echo "If the above list contains more than 1 entry, please consider splitting"
echo "your change into more commits, unless those changes don't make sense "
echo "individually (they do not build, tests do not pass, etc)."
echo "For example, it's perfectly fine to use 'obj' prefix for one commit that"
echo "changes libpmemobj source code, its tests and documentation."

if [ -n "$files" ]; then
	echo
	echo "Uncategorized files:"
	echo "$files"
fi
