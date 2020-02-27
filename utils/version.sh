#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2020, Intel Corporation
#
# utils/version.sh -- determine project's version
#
set -e

if [ -f "$1/VERSION" ]; then
	cat "$1/VERSION"
	exit 0
fi

if [ -f $1/GIT_VERSION ]; then
	echo -n "\$Format:%h\$" | cmp -s $1/GIT_VERSION - && true
	if [ $? -eq 0 ]; then
		PARSE_GIT_VERSION=0
	else
		PARSE_GIT_VERSION=1
	fi
else
	PARSE_GIT_VERSION=0
fi

LATEST_RELEASE=$(cat $1/ChangeLog | grep "* Version" | cut -d " " -f 3 | sort -rd | head -n1)

if [ $PARSE_GIT_VERSION -eq 1 ]; then
	GIT_VERSION_HASH=$(cat $1/GIT_VERSION)

	if [ -n "$GIT_VERSION_HASH" ]; then
		echo "$LATEST_RELEASE+git.$GIT_VERSION_HASH"
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
	echo "$LATEST_RELEASE+git.$GIT_COMMIT"
	exit 0
fi

cd - >/dev/null

# If nothing works, try to get version from directory name
VER=$(basename `realpath "$1"` | sed 's/pmdk[-]*\([0-9a-z.+-]*\).*/\1/')
if [ -n "$VER" ]; then
	echo "$VER"
	exit 0
fi

exit 1
