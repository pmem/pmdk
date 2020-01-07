#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# build-image.sh <OS-VER> - prepares a Docker image with <OS>-based
#                           environment for building PMDK project, according
#                           to the Dockerfile.<OS-VER> file located
#                           in the same directory.
#
# The script can be run locally.
#

set -e

function usage {
	echo "Usage:"
	echo "    build-image.sh <OS-VER>"
	echo "where <OS-VER>, for example, can be 'ubuntu-16.04', provided " \
		"a Dockerfile named 'Dockerfile.ubuntu-16.04' exists in the " \
		"current directory."
}

# Check if the first argument is nonempty
if [[ -z "$1" ]]; then
	usage
	exit 1
fi

# Check if the file Dockerfile.OS-VER exists
if [[ ! -f "Dockerfile.$1" ]]; then
	echo "ERROR: wrong argument."
	usage
	exit 1
fi

if [[ -z "${DOCKERHUB_REPO}" ]]; then
	echo "DOCKERHUB_REPO environment variable is not set"
	exit 1
fi

# Build a Docker image tagged with ${DOCKERHUB_REPO}:OS-VER
tag=${DOCKERHUB_REPO}:1.9-$1
docker build -t $tag \
	--build-arg http_proxy=$http_proxy \
	--build-arg https_proxy=$https_proxy \
	-f Dockerfile.$1 .
