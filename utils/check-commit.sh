#!/usr/bin/env bash
#
# Copyright 2016-2019, Intel Corporation
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
# usage: ./check-commit.sh [commit]...
#

if [ -z "$1" ]; then
	if [ -z "$TRAVIS_REPO_SLUG" -o -z "$GITHUB_REPO" -o -z "$TRAVIS_EVENT_TYPE" ]; then
		echo "Usage: $0 [commit]..."
		exit 1
	fi

	if [[ "$TRAVIS_REPO_SLUG" != "$GITHUB_REPO" \
		|| $TRAVIS_EVENT_TYPE != "pull_request" ]];
	then
		echo "SKIP: $0 can only be executed for pull requests to $GITHUB_REPO"
		exit 0
	fi

	# Find all the commits for the current build
	if [[ -n "$TRAVIS_COMMIT_RANGE" ]]; then
		MERGE_BASE=$(echo $TRAVIS_COMMIT_RANGE | cut -d. -f1)
		[ -z $MERGE_BASE ] && \
			MERGE_BASE=$(git log --pretty="%cN:%H" | grep GitHub | head -n1 | cut -d: -f2)
		commits=$(git log --pretty=%H $MERGE_BASE..$TRAVIS_COMMIT)
	else
		commits=$TRAVIS_COMMIT
	fi
else
	commits="$*"
fi

# valid area names
AREAS="pmem\|rpmem\|log\|blk\|obj\|pool\|test\|benchmark\|examples\|vmem\|vmmalloc\|jemalloc\|doc\|common\|daxio\|pmreorder"

# Check commit message
for commit in $commits; do
	subject=$(git log --format="%s" -n 1 $commit)
	commit_len=$(git log --format="%s%n%b" -n 1 $commit | wc -L)
	prefix=$(echo $subject | sed -n "s/^\($AREAS\)\:.*/\1/p")

	if [[ $subject =~ ^Merge.* ]]; then
		# skip
		continue
	fi
	if [ "$prefix" = "" ]; then
		echo "FAIL: subject line in commit message does not contain valid area name"
		echo
		`dirname $0`/check-area.sh $commit
		exit 1
	fi

	if [ $commit_len -gt 73 ]; then
		echo "FAIL: commit message exceeds 72 chars per line (commit_len)"
		echo
		git log -n 1 $commit
		exit 1
	fi
done
