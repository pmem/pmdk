#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2021, Intel Corporation

#
# build-local.sh - runs a Docker container from a Docker image with environment
#                  prepared for building PMDK project and starts building PMDK.
#
# This script is for building PMDK locally (not on CI).
#
# Notes:
# - run this script from its location or set the variable 'HOST_WORKDIR' to
#   where the root of the PMDK project is on the host machine.
# - set variables 'OS' and 'OS_VER' properly to a system you want to build PMDK
#   on (for proper values take a look on the list of Dockerfiles at the
#   utils/docker/images directory), eg. OS=ubuntu, OS_VER=16.04.
# - set 'KEEP_TEST_CONFIG' variable to 1 if you do not want the tests to be
#   reconfigured (your current test configuration will be preserved and used).
# - tests with Device Dax are not supported by pcheck yet, so do not provide
#   these devices in your configuration.
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
export DOCKER_REPO=${DOCKER_REPO:-ghcr.io/pmem/pmdk}
export GITHUB_REPO=${GITHUB_REPO:-pmem/pmdk}

if [[ -z "$IMG_VER" ]]; then
	# set the IMG_VER variable - version of Docker images
	source $(dirname $0)/images/set-images-version.sh
fi

if [[ -z "$OS" || -z "$OS_VER" || -z "$IMG_VER" ]]; then
	echo "ERROR: The variables OS, OS_VER and IMG_VER have to be set " \
		"(eg. OS=ubuntu, OS_VER=16.04, IMG_VER=1.10)."
	exit 1
fi

if [[ -z "$HOST_WORKDIR" ]]; then
	HOST_WORKDIR=$(readlink -f ../..)
fi

if [[ "$KEEP_CONTAINER" != "1" ]]; then
	RM_SETTING=" --rm"
fi

imageName=${DOCKER_REPO}:${IMG_VER}-${OS}-${OS_VER}-${CI_CPU_ARCH}
containerName=pmdk-${OS}-${OS_VER}

if [[ $MAKE_PKG -eq 1 ]] ; then
	command="./run-build-package.sh"
else
	command="./run-build.sh"
fi

if [ -n "$DNS_SERVER" ]; then DNS_SETTING=" --dns=$DNS_SERVER "; fi
if [ -z "$NDCTL_ENABLE" ]; then ndctl_enable=; else ndctl_enable="--env NDCTL_ENABLE=$NDCTL_ENABLE"; fi
if [ -z "$PMEMSET_INSTALL" ]; then pmemset_install=; else pmemset_install="--env PMEMSET_INSTALL=$PMEMSET_INSTALL"; fi

WORKDIR=/pmdk
SCRIPTSDIR=$WORKDIR/utils/docker

# Check if we are running on a CI (Travis or GitHub Actions)
[ -n "$GITHUB_ACTIONS" -o -n "$TRAVIS" ] && CI_RUN="YES" || CI_RUN="NO"

echo Building ${OS}-${OS_VER}

# Run a container with
#  - environment variables set (--env)
#  - host directory containing PMDK source mounted (-v)
#  - a tmpfs /tmp with the necessary size and permissions (--tmpfs)*
#  - working directory set (-w)
#
# * We need a tmpfs /tmp inside docker but we cannot run it with --privileged
#   and do it from inside, so we do using this docker-run option.
#   By default --tmpfs add nosuid,nodev,noexec to the mount flags, we don't
#   want that and just to make sure we add the usually default rw,relatime just
#   in case docker change the defaults.
docker run --name=$containerName -ti \
    --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
	$RM_SETTING \
	$DNS_SETTING \
	--env http_proxy=$http_proxy \
	--env https_proxy=$https_proxy \
	--env CC=$PMDK_CC \
	--env CXX=$PMDK_CXX \
	--env VALGRIND=$VALGRIND \
	--env EXTRA_CFLAGS=$EXTRA_CFLAGS \
	--env EXTRA_CXXFLAGS=$EXTRA_CXXFLAGS \
	--env EXTRA_LDFLAGS=$EXTRA_LDFLAGS \
	--env REMOTE_TESTS=$REMOTE_TESTS \
	--env CONFIGURE_TESTS=$CONFIGURE_TESTS \
	--env TEST_BUILD=$TEST_BUILD \
	--env WORKDIR=$WORKDIR \
	--env EXPERIMENTAL=$EXPERIMENTAL \
	--env SCRIPTSDIR=$SCRIPTSDIR \
	--env KEEP_TEST_CONFIG=$KEEP_TEST_CONFIG \
	--env CI_RUN=$CI_RUN \
	--env BLACKLIST_FILE=$BLACKLIST_FILE \
	$ndctl_enable \
	$pmemset_install \
	--tmpfs /tmp:rw,relatime,suid,dev,exec,size=6G \
	-v $HOST_WORKDIR:$WORKDIR \
	-v /etc/localtime:/etc/localtime \
	$DAX_SETTING \
	-w $SCRIPTSDIR \
	$imageName $command
