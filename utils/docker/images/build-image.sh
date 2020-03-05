#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation

#
# build-image.sh <OS-VER-ARCH> - prepares a Docker image with <OS>-based
#                           environment for building PMDK project, according
#                           to the Dockerfile.<OS-VER> file located
#                           in the same directory.
#
# The script can be run locally.
#

set -e

function usage {
	echo "Usage:"
	echo "    build-image.sh <OS-VER-ARCH>"
	echo "where <OS-VER-ARCH>, for example, can be 'ubuntu-16.04-x86_64', "\
		"provided a Dockerfile named 'Dockerfile.ubuntu-16.04' "\
		"exists in the current directory."
}

# Check if the first argument is nonempty
if [[ -z "$1" ]]; then
	usage
	exit 1
fi

# Dockerfile's "extension"
EXT=$(echo "$1" | cut -d'-' -f1-2)

# Check if the file Dockerfile.OS-VER exists
if [[ ! -f "Dockerfile.$EXT" ]]; then
	echo "ERROR: Dockerfile.$EXT does not exist."
	usage
	exit 1
fi

if [[ -z "${DOCKERHUB_REPO}" ]]; then
	echo "DOCKERHUB_REPO environment variable is not set"
	exit 1
fi

# Build a Docker image tagged with ${DOCKERHUB_REPO}:OS-VER-ARCH
tag=${DOCKERHUB_REPO}:1.9-$1
docker build -t $tag \
	--build-arg http_proxy=$http_proxy \
	--build-arg https_proxy=$https_proxy \
	-f Dockerfile.$EXT .
