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
# pull-or-rebuild-image.sh - rebuilds the Docker image used in the
#                            current Travis build if necessary.
#
# The script rebuilds the Docker image if the Dockerfile for the current
# OS version (Dockerfile.${OS}-${OS_VER}) or any .sh script from the directory
# with Dockerfiles were modified and committed.
#
# If the Travis build is not of the "pull_request" type (i.e. in case of
# merge after pull_request) and it succeed, the Docker image should be pushed
# to the Docker Hub repository. An empty file is created to signal that to
# further scripts.
#
# If the Docker image does not have to be rebuilt, it will be pulled from
# Docker Hub.
#

set -e

source $(dirname $0)/set-vars.sh

function get_commit_range_from_last_merge {
	# get commit id of the last merge
	LAST_MERGE=$(git log --merges --pretty=%H -1)
	LAST_COMMIT=$(git log --pretty=%H -1)
	if [ "$LAST_MERGE" == "$LAST_COMMIT" ]; then
		# GitHub Actions commits its own merge in case of pull requests
		# so the first merge commit has to be skipped.
		LAST_MERGE=$(git log --merges --pretty=%H -2 | tail -n1)
	fi
	if [ "$LAST_MERGE" == "" ]; then
		# possible in case of shallow clones
		COMMIT_RANGE=""
	else
		COMMIT_RANGE="$LAST_MERGE..HEAD"
		# make sure it works now
		if ! git rev-list $COMMIT_RANGE >/dev/null; then
			COMMIT_RANGE=""
		fi
	fi
	echo $COMMIT_RANGE
}

if [[ "$TRAVIS_EVENT_TYPE" != "cron" && "$TRAVIS_BRANCH" != "coverity_scan" \
	&& "$COVERITY" -eq 1 ]]; then
	echo "INFO: Skip Coverity scan job if build is triggered neither by " \
		"'cron' nor by a push to 'coverity_scan' branch"
	exit 0
fi

if [[ ( "$TRAVIS_EVENT_TYPE" == "cron" || "$TRAVIS_BRANCH" == "coverity_scan" )\
	&& "$COVERITY" -ne 1 ]]; then
	echo "INFO: Skip regular jobs if build is triggered either by 'cron'" \
		" or by a push to 'coverity_scan' branch"
	exit 0
fi

if [[ -z "$OS" || -z "$OS_VER" ]]; then
	echo "ERROR: The variables OS and OS_VER have to be set properly " \
             "(eg. OS=ubuntu, OS_VER=16.04)."
	exit 1
fi

if [[ -z "$HOST_WORKDIR" ]]; then
	echo "ERROR: The variable HOST_WORKDIR has to contain a path to " \
		"the root of the PMDK project on the host machine"
	exit 1
fi

# TRAVIS_COMMIT_RANGE is usually invalid for force pushes - fix it when used
# with non-upstream repository
if [ -n "$TRAVIS_COMMIT_RANGE" -a "$TRAVIS_REPO_SLUG" != "$GITHUB_REPO" ]; then
	if ! git rev-list $TRAVIS_COMMIT_RANGE; then
		TRAVIS_COMMIT_RANGE=$(get_commit_range_from_last_merge)
	fi
fi

# Fix Travis commit range
if [ -n "$TRAVIS_COMMIT_RANGE" ]; then
	# $TRAVIS_COMMIT_RANGE contains "..." instead of ".."
	# https://github.com/travis-ci/travis-ci/issues/4596
	PR_COMMIT_RANGE="${TRAVIS_COMMIT_RANGE/.../..}"
fi

# Set the commit range in case of GitHub Actions
if [ -n "$GITHUB_ACTIONS" ]; then
	PR_COMMIT_RANGE=$(get_commit_range_from_last_merge)
fi

# Find all the commits for the current build
if [ -n "$PR_COMMIT_RANGE" ]; then
	commits=$(git rev-list $PR_COMMIT_RANGE)
elif [ -n "$TRAVIS" ]; then
	commits=$TRAVIS_COMMIT
elif [ -n "$GITHUB_ACTIONS" ]; then
	commits=$GITHUB_SHA
else
	commits=$(git log --pretty=%H -1)
fi

echo "Commits in the commit range:"
for commit in $commits; do echo $commit; done

# Get the list of files modified by the commits
files=$(for commit in $commits; do git diff-tree --no-commit-id --name-only \
	-r $commit; done | sort -u)
echo "Files modified within the commit range:"
for file in $files; do echo $file; done

# Path to directory with Dockerfiles and image building scripts
images_dir_name=images
base_dir=utils/docker/$images_dir_name

# Check if committed file modifications require the Docker image to be rebuilt
for file in $files; do
	# Check if modified files are relevant to the current build
	if [[ $file =~ ^($base_dir)\/Dockerfile\.($OS)-($OS_VER)$ ]] \
		|| [[ $file =~ ^($base_dir)\/.*\.sh$ ]]
	then
		# Rebuild Docker image for the current OS version
		echo "Rebuilding the Docker image for the Dockerfile.$OS-$OS_VER"
		pushd $images_dir_name
		./build-image.sh ${OS}-${OS_VER}
		popd

		# Check if the image has to be pushed to Docker Hub
		# (i.e. the build is triggered by commits to the $GITHUB_REPO
		# repository's stable-* or master branch, and the Travis build is not
		# of the "pull_request" type). In that case, create the empty
		# file.
		if [[ "$TRAVIS_REPO_SLUG" == "$GITHUB_REPO" \
			&& ($TRAVIS_BRANCH == stable-* || $TRAVIS_BRANCH == devel-* || $TRAVIS_BRANCH == master) \
			&& $TRAVIS_EVENT_TYPE != "pull_request" \
			&& $PUSH_IMAGE == "1" ]]
		then
			echo "The image will be pushed to Docker Hub"
			touch $CI_FILE_PUSH_IMAGE_TO_REPO
		else
			echo "Skip pushing the image to Docker Hub"
		fi

		if [[ $PUSH_IMAGE == "1" ]]
		then
			echo "Skip build package check if image has to be pushed"
			touch $CI_FILE_SKIP_BUILD_PKG_CHECK
		fi
		exit 0
	fi
done

# Getting here means rebuilding the Docker image is not required.
# Pull the image from Docker Hub.
docker pull ${DOCKERHUB_REPO}:1.9-${OS}-${OS_VER}
