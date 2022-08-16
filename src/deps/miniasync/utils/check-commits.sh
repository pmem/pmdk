#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2022, Intel Corporation

#
# Used to check whether all commit messages in a pull request
# follow the GIT and/or project's guidelines.
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
		RANGE=$MERGE_BASE..$CI_COMMIT
	else
		MERGE_BASE=$(git log --pretty="%cN:%H" | grep GitHub | head -n1 | cut -d: -f2)
		RANGE=$MERGE_BASE..HEAD
	fi
else
	RANGE="$1"
fi

COMMITS=$(git log --pretty=%H $RANGE)

set -e

for commit in $COMMITS; do
	`dirname $0`/check-commit.sh $commit
done
