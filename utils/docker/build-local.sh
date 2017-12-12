#!/usr/bin/env bash
#
# Copyright 2017, Intel Corporation
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
#                  prepared for building NVML project and starts building NVML
#
# this script is for building NVML locally (not on Travis)
#
# Notes:
# - run this script from its location or set the variable 'HOST_WORKDIR' to
#   where the root of the NVML project is on the host machine,
# - set variables 'OS' and 'OS_VER' properly to a system you want to build NVML
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
export EXTRA_CFLAGS=${EXTRA_CFLAGS:--DUSE_VALGRIND}
export EXTRA_CXXFLAGS=${EXTRA_CXXFLAGS:-}
export NVML_CC=${NVML_CC:-gcc}
export NVML_CXX=${NVML_CXX:-g++}
export USE_LLVM_LIBCPP=${USE_LLVM_LIBCPP:-}
export LIBCPP_LIBDIR=${LIBCPP_LIBDIR:-}
export LIBCPP_INCDIR=${LIBCPP_INCDIR:-}
export EXPERIMENTAL=${EXPERIMENTAL:-n}


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

imageName=pmem/nvml:${OS}-${OS_VER}
containerName=nvml-${OS}-${OS_VER}

if [[ $MAKE_PKG -eq 1 ]] ; then
	command="./run-build-package.sh"
else
	command="./run-build.sh"
fi

if [ -n "$DNS_SERVER" ]; then DNS_SETTING=" --dns=$DNS_SERVER "; fi

WORKDIR=/nvml
SCRIPTSDIR=$WORKDIR/utils/docker

echo Building ${OS}-${OS_VER}

# Run a container with
#  - environment variables set (--env)
#  - host directory containing nvml source mounted (-v)
#  - working directory set (-w)
docker run --privileged=true --name=$containerName -ti \
	$RM_SETTING \
	$DNS_SETTING \
	--env http_proxy=$http_proxy \
	--env https_proxy=$https_proxy \
	--env CC=$NVML_CC \
	--env CXX=$NVML_CXX \
	--env EXTRA_CFLAGS=$EXTRA_CFLAGS \
	--env EXTRA_CXXFLAGS=$EXTRA_CXXFLAGS \
	--env USE_LLVM_LIBCPP=$USE_LLVM_LIBCPP \
	--env LIBCPP_LIBDIR=$LIBCPP_LIBDIR \
	--env LIBCPP_INCDIR=$LIBCPP_INCDIR \
	--env REMOTE_TESTS=$REMOTE_TESTS \
	--env CONFIGURE_TESTS=$CONFIGURE_TESTS \
	--env TEST_BUILD=$TEST_BUILD \
	--env WORKDIR=$WORKDIR \
	--env EXPERIMENTAL=$EXPERIMENTAL \
	--env SCRIPTSDIR=$SCRIPTSDIR \
	--env CLANG_FORMAT=clang-format-3.8 \
	--env KEEP_TEST_CONFIG=$KEEP_TEST_CONFIG \
	-v $HOST_WORKDIR:$WORKDIR \
	-v /etc/localtime:/etc/localtime \
	$DAX_SETTING \
	-w $SCRIPTSDIR \
	$imageName $command
