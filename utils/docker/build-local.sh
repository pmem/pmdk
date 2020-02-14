#!/usr/bin/env bash
#
# Copyright 2017-2020, Intel Corporation
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
# build-local.sh - runs a Docker container from a Docker image with environment
#                  prepared for building PMDK project and starts building PMDK
#
# this script is for building PMDK locally (not on Travis)
#
# Notes:
# - run this script from its location or set the variable 'HOST_WORKDIR' to
#   where the root of the PMDK project is on the host machine,
# - set variables 'OS' and 'OS_VER' properly to a system you want to build PMDK
#   on (for proper values take a look on the list of Dockerfiles at the
#   utils/docker/images directory), eg. OS=ubuntu, OS_VER=16.04.
# - set 'KEEP_TEST_CONFIG' variable to 1 if you do not want the tests to be
#   reconfigured (your current test configuration will be preserved and used)
# - tests with Device Dax are not supported by pcheck yet, so do not provide
#   these devices in your configuration
#

set -e

# Environment variables that can be customized (default values are after dash):
export KEEP_CONTAINER=${KEEP_CONTAINER:-0}
export KEEP_TEST_CONFIG=${KEEP_TEST_CONFIG:-0}
export TEST_BUILD=${TEST_BUILD:-all}
export REMOTE_TESTS=${REMOTE_TESTS:-1}
export MAKE_PKG=${MAKE_PKG:-0}
export EXTRA_CFLAGS=${EXTRA_CFLAGS}
export EXTRA_CXXFLAGS=${EXTRA_CXXFLAGS:-}
export PMDK_CC=${PMDK_CC:-gcc}
export PMDK_CXX=${PMDK_CXX:-g++}
export EXPERIMENTAL=${EXPERIMENTAL:-n}
export VALGRIND=${VALGRIND:-1}
export DOCKERHUB_REPO=${DOCKERHUB_REPO:-pmem/pmdk}
export GITHUB_REPO=${GITHUB_REPO:-pmem/pmdk}

if [[ -z "$OS" || -z "$OS_VER" ]]; then
	echo "ERROR: The variables OS and OS_VER have to be set " \
		"(eg. OS=ubuntu, OS_VER=16.04)."
	exit 1
fi

if [[ -z "$HOST_WORKDIR" ]]; then
	HOST_WORKDIR=$(readlink -f ../..)
fi

if [[ "$KEEP_CONTAINER" != "1" ]]; then
	RM_SETTING=" --rm"
fi

imageName=${DOCKERHUB_REPO}:1.7-${OS}-${OS_VER}
containerName=pmdk-${OS}-${OS_VER}

if [[ $MAKE_PKG -eq 1 ]] ; then
	command="./run-build-package.sh"
else
	command="./run-build.sh"
fi

if [ -n "$DNS_SERVER" ]; then DNS_SETTING=" --dns=$DNS_SERVER "; fi
if [ -z "$NDCTL_ENABLE" ]; then ndctl_enable=; else ndctl_enable="--env NDCTL_ENABLE=$NDCTL_ENABLE"; fi

WORKDIR=/pmdk
SCRIPTSDIR=$WORKDIR/utils/docker

# Check if we are running on a CI (Travis or GitHub Actions)
[ -n "$GITHUB_ACTIONS" -o -n "$TRAVIS" ] && CI_RUN="YES" || CI_RUN="NO"

echo Building ${OS}-${OS_VER}

# Run a container with
#  - environment variables set (--env)
#  - host directory containing PMDK source mounted (-v)
#  - working directory set (-w)
docker run --privileged=true --name=$containerName -ti \
	$RM_SETTING \
	$DNS_SETTING \
	--env http_proxy=$http_proxy \
	--env https_proxy=$https_proxy \
	--env CC=$PMDK_CC \
	--env CXX=$PMDK_CXX \
	--env VALGRIND=$VALGRIND \
	--env EXTRA_CFLAGS=$EXTRA_CFLAGS \
	--env EXTRA_CXXFLAGS=$EXTRA_CXXFLAGS \
	--env REMOTE_TESTS=$REMOTE_TESTS \
	--env CONFIGURE_TESTS=$CONFIGURE_TESTS \
	--env TEST_BUILD=$TEST_BUILD \
	--env WORKDIR=$WORKDIR \
	--env EXPERIMENTAL=$EXPERIMENTAL \
	--env SCRIPTSDIR=$SCRIPTSDIR \
	--env KEEP_TEST_CONFIG=$KEEP_TEST_CONFIG \
	--env CI_RUN=$CI_RUN \
	$ndctl_enable \
	-v $HOST_WORKDIR:$WORKDIR \
	-v /etc/localtime:/etc/localtime \
	$DAX_SETTING \
	-w $SCRIPTSDIR \
	$imageName $command
