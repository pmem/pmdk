#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# Used to check whether all the commit messages in a pull request
# follow the GIT/PMDK guidelines.
#
# usage: ./check-commits.sh [range]
#

if [ -z "$1" ]; then
	# on Travis run this check only for pull requests
	if [ -n "$TRAVIS_REPO_SLUG" ]; then
		if [[ "$TRAVIS_REPO_SLUG" != "$GITHUB_REPO" \
			|| $TRAVIS_EVENT_TYPE != "pull_request" ]];
		then
			echo "SKIP: $0 can only be executed for pull requests to $GITHUB_REPO"
			exit 0
		fi
	fi

	last_merge=$(git log --pretty="%cN:%H" | grep GitHub | head -n1 | cut -d: -f2)
	range=${last_merge}..HEAD
else
	range="$1"
fi

commits=$(git log --pretty=%H $range)

set -e

for commit in $commits; do
	`dirname $0`/check-commit.sh $commit
done
