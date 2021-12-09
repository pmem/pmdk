#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2021, Intel Corporation
#

#
# build-image.sh <OS-VER> - prepares a Docker image with <OS>-based
#                           environment for testing miniasync, according
#                           to the Dockerfile.<OS-VER> file located
#                           in the same directory.
#
# The script can be run locally.
#

set -e

function usage {
	echo "Usage:"
	echo "    build-image.sh <DOCKER_REPO> <OS-VER>"
	echo "where <OS-VER>, for example, can be 'fedora-30', provided " \
		"a Dockerfile named 'Dockerfile.fedora-30' exists in the " \
		"current directory."
}

# Check if the first and second argument is nonempty
if [[ -z "$1" || -z "$2" ]]; then
	usage
	exit 1
fi

# Check if the file Dockerfile.OS-VER exists
if [[ ! -f "Dockerfile.$2" ]]; then
	echo "ERROR: wrong argument."
	usage
	exit 1
fi

# Build a Docker image tagged with ${DOCKER_REPO}:${IMG_VER}-${OS}-${OS_VER}
docker build -t $1:${IMG_VER}-$2 \
	--build-arg http_proxy=$http_proxy \
	--build-arg https_proxy=$https_proxy \
	-f Dockerfile.$2 .
