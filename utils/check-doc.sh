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
# Used to check whether changes to the generated documentation directory
# are made by the authorised user. Used only by travis builds.
#
# usage: ./check-doc.sh
#

directory=doc/generated
allowed_user="pmem-bot <pmem-bot@intel.com>"

if [[ -z "$TRAVIS" ]]; then
	echo "ERROR: $0 can only be executed on Travis CI."
	exit 1
fi

if [[ "$TRAVIS_REPO_SLUG" != "$GITHUB_REPO" \
	|| $TRAVIS_EVENT_TYPE != "pull_request" ]];
then
	echo "SKIP: $0 can only be executed for pull requests to ${GITHUB_REPO}"
	exit 0
fi

# Find all the commits for the current build
if [[ -n "$TRAVIS_COMMIT_RANGE" ]]; then
	# $TRAVIS_COMMIT_RANGE contains "..." instead of ".."
	# https://github.com/travis-ci/travis-ci/issues/4596
	PR_COMMIT_RANGE="${TRAVIS_COMMIT_RANGE/.../..}"

	commits=$(git rev-list $PR_COMMIT_RANGE)
else
	commits=$TRAVIS_COMMIT
fi

# Check for changes in the generated docs directory
# Only new files are allowed (first version)
for commit in $commits; do
	last_author=$(git --no-pager show -s --format='%aN <%aE>' $commit)
	if [ "$last_author" == "$allowed_user" ]; then
		continue
	fi

	fail=$(git diff-tree --no-commit-id --name-status -r $commit | grep -c ^M.*$directory)
	if [ $fail -ne 0 ]; then
		echo "FAIL: changes to ${directory} allowed only by \"${allowed_user}\""
		exit 1
	fi
done
