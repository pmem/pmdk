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
# push-image.sh <OS-VER> - pushes the Docker image tagged with OS-VER
#                          to the Docker Hub.
#
# The script utilizes $DOCKER_USER and $DOCKER_PASSWORD variables to log in to
# Docker Hub. The variables can be set in the Travis project's configuration
# for automated builds.
#

set -e

function usage {
	echo "Usage:"
	echo "    push-image.sh <OS-VER>"
	echo "where <OS-VER>, for example, can be 'ubuntu-16.04', provided " \
		"a Docker image tagged with ${DOCKERHUB_REPO}:ubuntu-16.04 exists " \
		"locally."
}

# Check if the first argument is nonempty
if [[ -z "$1" ]]; then
	usage
	exit 1
fi

if [[ -z "${DOCKERHUB_REPO}" ]]; then
	echo "DOCKERHUB_REPO environment variable is not set"
	exit 1
fi

# Check if the image tagged with pmdk/OS-VER exists locally
if [[ ! $(docker images -a | awk -v pattern="^${DOCKERHUB_REPO}:1.9-$1\$" \
	'$1":"$2 ~ pattern') ]]
then
	echo "ERROR: wrong argument."
	usage
	exit 1
fi

# Log in to the Docker Hub
docker login -u="$DOCKER_USER" -p="$DOCKER_PASSWORD"

# Push the image to the repository
docker push ${DOCKERHUB_REPO}:1.9-$1
