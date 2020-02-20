#!/usr/bin/env bash
#
# Copyright 2016-2020, Intel Corporation
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
	# TRAVIS_COMMIT_RANGE can be invalid for force pushes - use another
	# method to determine the list of commits
	if [[ $(git rev-list $TRAVIS_COMMIT_RANGE) || -n "$TRAVIS_COMMIT_RANGE" ]]; then
		MERGE_BASE=$(echo $TRAVIS_COMMIT_RANGE | cut -d. -f1)
		[ -z $MERGE_BASE ] && \
			MERGE_BASE=$(git log --pretty="%cN:%H" | grep GitHub | head -n1 | cut -d: -f2)
		commits=$(git log --pretty=%H $MERGE_BASE..$TRAVIS_COMMIT)
		echo "\n\n\n111111111111111111111111111111111111111111111\n\n\n"
	else
		last_merge=$(git log --pretty="%cN:%H" | grep GitHub | head -n1 | cut -d: -f2)
		range=${last_merge}..HEAD
	fi
else
	range="$1"
fi

if [ -z $commits ]; then
	commits=$(git log --pretty=%H $range)
fi
echo "\n\n$commits\n\n"

set -e

for commit in $commits; do
	`dirname $0`/check-commit.sh $commit
done
