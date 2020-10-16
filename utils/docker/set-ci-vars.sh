#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# set-ci-vars.sh -- set CI variables common for both:
#                   Travis and GitHub Actions CIs
#

set -e

# set version of Docker images (IMG_VER)
source $(dirname ${BASH_SOURCE[0]})/images/set-images-version.sh

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
		# or new repos with no merge commits yet
		# - pick up the first commit
		LAST_MERGE=$(git log --pretty=%H | tail -n1)
	fi
	COMMIT_RANGE="$LAST_MERGE..HEAD"
	# make sure it works now
	if ! git rev-list $COMMIT_RANGE >/dev/null; then
		COMMIT_RANGE=""
	fi
	echo $COMMIT_RANGE
}

COMMIT_RANGE_FROM_LAST_MERGE=$(get_commit_range_from_last_merge)

if [ -n "$TRAVIS" ]; then
	CI_COMMIT=$TRAVIS_COMMIT
	CI_COMMIT_RANGE="${TRAVIS_COMMIT_RANGE/.../..}"
	CI_BRANCH=$TRAVIS_BRANCH
	CI_EVENT_TYPE=$TRAVIS_EVENT_TYPE
	CI_REPO_SLUG=$TRAVIS_REPO_SLUG

	# CI_COMMIT_RANGE is usually invalid for force pushes - fix it when used
	# with non-upstream repository
	if [ -n "$CI_COMMIT_RANGE" -a "$CI_REPO_SLUG" != "$GITHUB_REPO" ]; then
		if ! git rev-list $CI_COMMIT_RANGE; then
			CI_COMMIT_RANGE=$COMMIT_RANGE_FROM_LAST_MERGE
		fi
	fi

	case "$TRAVIS_CPU_ARCH" in
	"amd64")
		CI_CPU_ARCH="x86_64"
		;;
	*)
		CI_CPU_ARCH=$TRAVIS_CPU_ARCH
		;;
	esac

elif [ -n "$GITHUB_ACTIONS" ]; then
	CI_COMMIT=$GITHUB_SHA
	CI_COMMIT_RANGE=$COMMIT_RANGE_FROM_LAST_MERGE
	CI_BRANCH=$(echo $GITHUB_REF | cut -d'/' -f3)
	CI_REPO_SLUG=$GITHUB_REPOSITORY
	CI_CPU_ARCH="x86_64" # GitHub Actions supports only x86_64

	case "$GITHUB_EVENT_NAME" in
	"schedule")
		CI_EVENT_TYPE="cron"
		;;
	*)
		CI_EVENT_TYPE=$GITHUB_EVENT_NAME
		;;
	esac

else
	CI_COMMIT=$(git log --pretty=%H -1)
	CI_COMMIT_RANGE=$COMMIT_RANGE_FROM_LAST_MERGE
	CI_CPU_ARCH="x86_64"
fi

export CI_COMMIT=$CI_COMMIT
export CI_COMMIT_RANGE=$CI_COMMIT_RANGE
export CI_BRANCH=$CI_BRANCH
export CI_EVENT_TYPE=$CI_EVENT_TYPE
export CI_REPO_SLUG=$CI_REPO_SLUG
export CI_CPU_ARCH=$CI_CPU_ARCH
export IMG_VER=$IMG_VER

echo CI_COMMIT=$CI_COMMIT
echo CI_COMMIT_RANGE=$CI_COMMIT_RANGE
echo CI_BRANCH=$CI_BRANCH
echo CI_EVENT_TYPE=$CI_EVENT_TYPE
echo CI_REPO_SLUG=$CI_REPO_SLUG
echo CI_CPU_ARCH=$CI_CPU_ARCH
echo IMG_VER=$IMG_VER
