#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation

#
# push-image.sh - pushes the Docker image to GitHub Container Registry.
#
# The script utilizes $GH_CR_USER and $GH_CR_PAT variables
# to log in to GitHub Container Registry. The variables can be set
# in the Travis project's configuration for automated builds.
#

set -e

source $(dirname $0)/../set-ci-vars.sh

if [[ -z "$OS" ]]; then
	echo "OS environment variable is not set"
	exit 1
fi

if [[ -z "$OS_VER" ]]; then
	echo "OS_VER environment variable is not set"
	exit 1
fi

if [[ -z "$CI_CPU_ARCH" ]]; then
	echo "CI_CPU_ARCH environment variable is not set"
	exit 1
fi

if [[ -z "${DOCKER_REPO}" ]]; then
	echo "DOCKER_REPO environment variable is not set"
	exit 1
fi

TAG="1.10-${OS}-${OS_VER}-${CI_CPU_ARCH}"

# Check if the image tagged with pmdk/OS-VER exists locally
if [[ ! $(docker images -a | awk -v pattern="^${DOCKER_REPO}:${TAG}\$" \
	'$1":"$2 ~ pattern') ]]
then
	echo "ERROR: Docker image tagged ${DOCKER_REPO}:${TAG} does not exists locally."
	exit 1
fi

# Log in to GitHub Container Registry
docker login https://ghcr.io -u="$GH_CR_USER" -p="$GH_CR_PAT"

# Push the image to GitHub Container Registry
docker push ${DOCKER_REPO}:${TAG}
