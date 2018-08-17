#!/bin/bash -e
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
# build-travis.sh - runs a Docker container from a Docker image with environment
#                   prepared for building NVML project and starts building NVML
#
# this script is for building NVML on Travis only
#

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
		"the root of the nvml project on the host machine"
	exit 1
fi

if [[ -z "$TEST_BUILD" ]]; then
	TEST_BUILD=all
fi

imageName=pmem/pmdk:1.3-${OS}-${OS_VER}
containerName=pmdk-${OS}-${OS_VER}

if [[ $MAKE_PKG -eq 0 ]] ; then command="./run-build.sh"; fi
if [[ $MAKE_PKG -eq 1 ]] ; then command="./run-build-package.sh"; fi
if [[ $COVERAGE -eq 1 ]] ; then command="./run-coverage.sh"; ci_env=`bash <(curl -s https://codecov.io/env)`; fi

if [[ ( "$TRAVIS_EVENT_TYPE" == "cron" || "$TRAVIS_BRANCH" == "coverity_scan" )\
	&& "$COVERITY" -eq 1 ]]; then
	command="./run-coverity.sh"
fi

if [ -n "$DNS_SERVER" ]; then DNS_SETTING=" --dns=$DNS_SERVER "; fi

WORKDIR=/nvml
SCRIPTSDIR=$WORKDIR/utils/docker

# Run a container with
#  - environment variables set (--env)
#  - host directory containing nvml source mounted (-v)
#  - working directory set (-w)
docker run --rm --privileged=true --name=$containerName -ti \
	$DNS_SETTING \
	$ci_env \
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
	--env TEST_BUILD=$TEST_BUILD \
	--env WORKDIR=$WORKDIR \
	--env EXPERIMENTAL=$EXPERIMENTAL \
	--env SCRIPTSDIR=$SCRIPTSDIR \
	--env CLANG_FORMAT=clang-format-3.8 \
	--env TRAVIS=$TRAVIS \
	--env TRAVIS_COMMIT_RANGE=$TRAVIS_COMMIT_RANGE \
	--env TRAVIS_COMMIT=$TRAVIS_COMMIT \
	--env TRAVIS_REPO_SLUG=$TRAVIS_REPO_SLUG \
	--env TRAVIS_BRANCH=$TRAVIS_BRANCH \
	--env TRAVIS_EVENT_TYPE=$TRAVIS_EVENT_TYPE \
	--env COVERITY_SCAN_TOKEN=$COVERITY_SCAN_TOKEN \
	--env COVERITY_SCAN_NOTIFICATION_EMAIL=$COVERITY_SCAN_NOTIFICATION_EMAIL \
	-v $HOST_WORKDIR:$WORKDIR \
	-v /etc/localtime:/etc/localtime \
	-w $SCRIPTSDIR \
	$imageName $command
