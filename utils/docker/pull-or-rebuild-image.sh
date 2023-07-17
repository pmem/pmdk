#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2023, Intel Corporation

#
# pull-or-rebuild-image.sh - rebuilds the Docker image used in the
#		current build (if necessary) or pulls it from the $DOCKER_REPO.
#
# Usage: pull-or-rebuild-image.sh [rebuild|pull]
#
# If Docker was rebuilt and all requirements are fulfilled (more details in
# push_image function below) image will be pushed to the $DOCKER_REPO.
#
# The script rebuilds the Docker image if:
# 1. the Dockerfile for the current OS version (Dockerfile.${OS}-${OS_VER})
#    or any .sh script in the Dockerfiles directory were modified and committed, or
# 2. "rebuild" param was passed as a first argument to this script.
#
# The script pulls the Docker image if:
# 1. it does not have to be rebuilt (based on committed changes), or
# 2. "pull" param was passed as a first argument to this script.
#

set -e

source $(dirname $0)/set-ci-vars.sh
source $(dirname $0)/set-vars.sh

# Path to directory with Dockerfiles and image building scripts
images_dir_name=images
base_dir=utils/docker/$images_dir_name

if [[ -z "$OS" || -z "$OS_VER" ]]; then
	echo "ERROR: The variables OS and OS_VER have to be set properly " \
             "(eg. OS=ubuntu, OS_VER=22.04)."
	exit 1
fi

if [[ -z "${DOCKER_REPO}" ]]; then
	echo "ERROR: DOCKER_REPO environment variable is not set " \
		"(e.g. \"<docker_repo_addr>/<org_name>/<package_name>\")."
	exit 1
fi

function build_image() {
	echo "Building the Docker image for the Dockerfile.${OS}-${OS_VER}"
	pushd $images_dir_name
	./build-image.sh ${OS}-${OS_VER} ${CI_CPU_ARCH}
	popd
}

function pull_image() {
	echo "Pull the image from the DOCKER_REPO."
	docker pull ${DOCKER_REPO}:${IMG_VER}-${OS}-${OS_VER}-${CI_CPU_ARCH}
}

function push_image {
	# Check if the image has to be pushed to the DOCKER_REPO:
	# - only upstream (not forked) repository,
	# - stable-*, devel-*, or master branch,
	# - not a pull_request event,
	# - and PUSH_IMAGE flag was set for current build.
	if [[ "${CI_REPO_SLUG}" == "${GITHUB_REPO}" \
		&& (((${CI_BRANCH} == stable-* || ${CI_BRANCH} == devel-* || ${CI_BRANCH} == master) \
		&& ${CI_EVENT_TYPE} != "pull_request") \
			|| ${PUSH_IMAGE} == "1") ]]
	then
		echo "The image will be pushed to the Container Registry: ${DOCKER_REPO}"
		pushd ${images_dir_name}
		./push-image.sh
		popd
	else
		echo "Skip pushing the image to the ${DOCKER_REPO}."
		echo "CI_REPO_SLUG: ${CI_REPO_SLUG}"
		echo "GITHUB_REPO: ${GITHUB_REPO}"
		echo "CI_BRANCH: ${CI_BRANCH}"
		echo "CI_EVENT_TYPE: ${CI_EVENT_TYPE}"
		echo "PUSH_IMAGE: ${PUSH_IMAGE}"
	fi
}

# If "rebuild" or "pull" are passed to the script as param, force rebuild/pull.
if [[ "${1}" == "rebuild" ]]; then
	build_image
	push_image
	exit 0
elif [[ "${1}" == "pull" ]]; then
	pull_image
	exit 0
fi

#
# Determine if we need to rebuild the image or just pull it from
# the DOCKER_REPO, based on committed changes.
#

# Find all the commits for the current build
if [ -n "$CI_COMMIT_RANGE" ]; then
	commits=$(git rev-list $CI_COMMIT_RANGE)
else
	commits=$CI_COMMIT
fi

echo "Commits in the commit range:"
for commit in $commits; do echo $commit; done

echo "Files modified within the commit range:"
files=$(for commit in $commits; do git diff-tree --no-commit-id --name-only \
	-r $commit; done | sort -u)
for file in $files; do echo $file; done

# Check if committed file modifications require the Docker image to be rebuilt
for file in $files; do
	# Check if modified files are relevant to the current build
	if [[ $file =~ ^($base_dir)\/Dockerfile\.($OS)-($OS_VER)$ ]] \
		|| [[ $file =~ ^($base_dir)\/.*\.sh$ ]] \
		|| [[ $file =~ ^($base_dir)\/\.\.\/\.\.\/\.\.\/\.github\/workflows\/docker_rebuild\.yml ]]
	then
		build_image
		push_image

		if [[ $PUSH_IMAGE == "1" ]]
		then
			echo "Skip build package check if image has to be pushed"
			touch $CI_FILE_SKIP_BUILD_PKG_CHECK
		fi
		exit 0
	fi
done

# Getting here means rebuilding the Docker image isn't required (based on changed files).
# Pull the image from the $DOCKER_REPO or rebuild anyway, if pull fails.
if ! pull_image; then
	build_image
	push_image
fi
