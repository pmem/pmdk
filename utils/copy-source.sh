#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation

#
# utils/copy-source.sh -- copy source files (from HEAD) to 'path_to_dir/pmdk'
# directory whether in git repository or not.
#
# usage: ./copy-source.sh [path_to_dir] [srcversion]

set -e

DESTDIR="$1"
SRCVERSION=$2

if [ -d .git ]; then
	if [ -n "$(git status --porcelain)" ]; then
		echo "Error: Working directory is dirty: $(git status --porcelain)"
		exit 1
	fi
else
	echo "Warning: You are not in git repository, working directory might be dirty."
fi

mkdir -p "$DESTDIR"/pmdk
echo -n $SRCVERSION > "$DESTDIR"/pmdk/.version

if [ -d .git ]; then
	git archive HEAD | tar -x -C "$DESTDIR"/pmdk
else
	find . \
	-maxdepth 1 \
	-not -name $(basename "$DESTDIR") \
	-not -name . \
	-exec cp -r "{}" "$DESTDIR"/pmdk \;
fi
