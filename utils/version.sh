#!/usr/bin/env bash
#
# Copyright 2017-2019, Intel Corporation
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
# utils/version.sh -- determine project's version
#
set -e

if [ -f "$1/VERSION" ]; then
	cat "$1/VERSION"
	exit 0
fi

if [ -f $1/GIT_VERSION ]; then
	echo -n "\$Format:%h %d\$" | cmp -s $1/GIT_VERSION - && true
	if [ $? -eq 0 ]; then
		PARSE_GIT_VERSION=0
	else
		PARSE_GIT_VERSION=1
	fi
else
	PARSE_GIT_VERSION=0
fi

if [ $PARSE_GIT_VERSION -eq 1 ]; then
	GIT_VERSION_TAG=$(cat $1/GIT_VERSION | grep tag: | sed 's/.*tag: \([0-9a-z.+-]*\).*/\1/')
	GIT_VERSION_HASH=$(cat $1/GIT_VERSION | sed -e 's/ .*//')

	if [ -n "$GIT_VERSION_TAG" ]; then
		echo "$GIT_VERSION_TAG"
		exit 0
	fi

	if [ -n "$GIT_VERSION_HASH" ]; then
		echo "$GIT_VERSION_HASH"
		exit 0
	fi
fi

cd "$1"

GIT_DESCRIBE=$(git describe 2>/dev/null) && true
if [ -n "$GIT_DESCRIBE" ]; then
	# 1.5-19-gb8f78a329 -> 1.5+git19.gb8f78a329
	# 1.5-rc1-19-gb8f78a329 -> 1.5-rc1+git19.gb8f78a329
	echo "$GIT_DESCRIBE" | sed "s/\([0-9.]*\)-rc\([0-9]*\)-\([0-9]*\)-\([0-9a-g]*\)/\1-rc\2+git\3.\4/" | sed "s/\([0-9.]*\)-\([0-9]*\)-\([0-9a-g]*\)/\1+git\2.\3/"
	exit 0
fi

# try commit it, git describe can fail when there are no tags (e.g. with shallow clone, like on Travis)
GIT_COMMIT=$(git log -1 --format=%h) && true
if [ -n "$GIT_COMMIT" ]; then
	echo "$GIT_COMMIT"
	exit 0
fi

cd - >/dev/null

# If nothing works, try to get version from directory name
VER=$(basename `realpath "$1"` | sed 's/vmem[-]*\([0-9a-z.+-]*\).*/\1/')
if [ -n "$VER" ]; then
	echo "$VER"
	exit 0
fi

exit 1
