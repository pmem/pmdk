#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation

#
# Used to check whether all the commit messages in a pull request
# follow the GIT/PMDK guidelines.
#
# usage: ./check-commits.sh [range]
#

if [ -z "$1" ]; then
	# on CI run this check only for pull requests
	if [ -n "$CI_REPO_SLUG" ]; then
		if [[ "$CI_REPO_SLUG" != "$GITHUB_REPO" \
			|| $CI_EVENT_TYPE != "pull_request" ]];
		then
			echo "SKIP: $0 can only be executed for pull requests to $GITHUB_REPO"
			exit 0
		fi
	fi
	# CI_COMMIT_RANGE can be invalid for force pushes - use another
	# method to determine the list of commits
	if [[ $(git rev-list $CI_COMMIT_RANGE 2>/dev/null) || -n "$CI_COMMIT_RANGE" ]]; then
		MERGE_BASE=$(echo $CI_COMMIT_RANGE | cut -d. -f1)
		[ -z $MERGE_BASE ] && \
			MERGE_BASE=$(git log --pretty="%cN:%H" | grep GitHub | head -n1 | cut -d: -f2)
		range=$MERGE_BASE..$CI_COMMIT
	else
		merge_base=$(git log --pretty="%cN:%H" | grep GitHub | head -n1 | cut -d: -f2)
		range=$merge_base..HEAD
	fi
else
	range="$1"
fi

commits=$(git log --pretty=%H $range)

set -e

for commit in $commits; do
	`dirname $0`/check-commit.sh $commit
done
