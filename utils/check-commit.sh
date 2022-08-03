#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2022, Intel Corporation

#
# Used to check whether all the commit messages in a pull request
# follow the GIT/PMDK guidelines.
#
# usage: ./check-commit.sh commit
#

if [ -z "$1" ]; then
	echo "Usage: check-commit.sh commit-id"
	exit 1
fi

echo "Checking $1"

subject=$(git log --format="%s" -n 1 $1)
body=$(git log --format="%b" -n 1 $1)

if [[ $subject =~ ^Merge.* ]]; then
	# skip
	exit 0
fi

if [[ $subject =~ ^Revert.* ]]; then
	# skip
	exit 0
fi

if [[ $body =~ "git-subtree-dir: src/deps/miniasync" ]]; then
	# skip
	exit 0
fi

# valid area names
AREAS="pmem\|pmem2\|log\|blk\|obj\|pool\|set\|test\|benchmark\|examples\|doc\|core\|common\|daxio\|pmreorder"

prefix=$(echo $subject | sed -n "s/^\($AREAS\)\:.*/\1/p")

if [ "$prefix" = "" ]; then
	echo "FAIL: subject line in commit message does not contain valid area name"
	echo
	`dirname $0`/check-area.sh $1
	exit 1
fi

ignore_long_link_lines="!/^http/"
commit_len=$(git log --format="%s%n%b" -n 1 $1 | awk ${ignore_long_link_lines} | wc -L)

if [ $commit_len -gt 73 ]; then
	echo "FAIL: commit message exceeds 72 chars per line (commit_len)"
	echo
	git log -n 1 $1 | cat
	exit 1
fi
