#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation

#
# build-travis.sh - runs a Docker container from a Docker image with environment
#                   prepared for building PMDK project and starts building PMDK
#
# this script is for building PMDK on Travis only
#

set -e

source $(dirname $0)/set-ci-vars.sh
source $(dirname $0)/set-vars.sh
source $(dirname $0)/valid-branches.sh

if [[ "$CI_EVENT_TYPE" != "cron" && "$CI_BRANCH" != "coverity_scan" \
	&& "$COVERITY" -eq 1 ]]; then
	echo "INFO: Skip Coverity scan job if build is triggered neither by " \
		"'cron' nor by a push to 'coverity_scan' branch"
	exit 0
fi

if [[ ( "$CI_EVENT_TYPE" == "cron" || "$CI_BRANCH" == "coverity_scan" )\
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
		"the root of the PMDK project on the host machine"
	exit 1
fi

if [[ -z "$TEST_BUILD" ]]; then
	TEST_BUILD=all
fi

imageName=${DOCKERHUB_REPO}:1.9-${OS}-${OS_VER}-${CI_CPU_ARCH}
containerName=pmdk-${OS}-${OS_VER}

if [[ $MAKE_PKG -eq 0 ]] ; then command="./run-build.sh"; fi
if [[ $MAKE_PKG -eq 1 ]] ; then command="./run-build-package.sh"; fi
if [[ $COVERAGE -eq 1 ]] ; then command="./run-coverage.sh"; ci_env=`bash <(curl -s https://codecov.io/env)`; fi

if [[ ( "$CI_EVENT_TYPE" == "cron" || "$CI_BRANCH" == "coverity_scan" )\
	&& "$COVERITY" -eq 1 ]]; then
	command="./run-coverity.sh"
fi

if [ -n "$DNS_SERVER" ]; then DNS_SETTING=" --dns=$DNS_SERVER "; fi
if [[ -f $CI_FILE_SKIP_BUILD_PKG_CHECK ]]; then BUILD_PACKAGE_CHECK=n; else BUILD_PACKAGE_CHECK=y; fi
if [ -z "$NDCTL_ENABLE" ]; then ndctl_enable=; else ndctl_enable="--env NDCTL_ENABLE=$NDCTL_ENABLE"; fi
if [[ $UBSAN -eq 1 ]]; then for x in C CPP LD; do declare EXTRA_${x}FLAGS=-fsanitize=undefined; done; fi

# Only run doc update on $GITHUB_REPO master or stable branch
if [[ -z "${CI_BRANCH}" || -z "${TARGET_BRANCHES[${CI_BRANCH}]}" || "$CI_EVENT_TYPE" == "pull_request" || "$CI_REPO_SLUG" != "${GITHUB_REPO}" ]]; then
	AUTO_DOC_UPDATE=0
fi

# Check if we are running on a CI (Travis or GitHub Actions)
[ -n "$GITHUB_ACTIONS" -o -n "$TRAVIS" ] && CI_RUN="YES" || CI_RUN="NO"

# We have a blacklist only for ppc64le arch
if [[ "$CI_CPU_ARCH" == ppc64le ]] ; then BLACKLIST_FILE=../../utils/docker/ppc64le.blacklist; fi

# docker on travis + ppc64le runs inside an LXD container and for security
# limits what can be done inside it, and as such, `docker run` fails with
# > the input device is not a TTY
# when using -t because of limited permissions to /dev imposed by LXD.
if [[ -n "$TRAVIS" && "$CI_CPU_ARCH" == ppc64le ]] || [[ -n "$GITHUB_ACTIONS" ]]; then
	TTY=''
else
	TTY='-t'
fi

WORKDIR=/pmdk
SCRIPTSDIR=$WORKDIR/utils/docker

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
docker run --rm --name=$containerName -i $TTY \
	$DNS_SETTING \
	$ci_env \
	--env http_proxy=$http_proxy \
	--env https_proxy=$https_proxy \
	--env AUTO_DOC_UPDATE=$AUTO_DOC_UPDATE \
	--env CC=$PMDK_CC \
	--env CXX=$PMDK_CXX \
	--env VALGRIND=$VALGRIND \
	--env EXTRA_CFLAGS=$EXTRA_CFLAGS \
	--env EXTRA_CXXFLAGS=$EXTRA_CXXFLAGS \
	--env EXTRA_LDFLAGS=$EXTRA_LDFLAGS \
	--env REMOTE_TESTS=$REMOTE_TESTS \
	--env TEST_BUILD=$TEST_BUILD \
	--env WORKDIR=$WORKDIR \
	--env EXPERIMENTAL=$EXPERIMENTAL \
	--env BUILD_PACKAGE_CHECK=$BUILD_PACKAGE_CHECK \
	--env SCRIPTSDIR=$SCRIPTSDIR \
	--env TRAVIS=$TRAVIS \
	--env CI_COMMIT_RANGE=$CI_COMMIT_RANGE \
	--env CI_COMMIT=$CI_COMMIT \
	--env CI_REPO_SLUG=$CI_REPO_SLUG \
	--env CI_BRANCH=$CI_BRANCH \
	--env CI_EVENT_TYPE=$CI_EVENT_TYPE \
	--env DOC_UPDATE_GITHUB_TOKEN=$DOC_UPDATE_GITHUB_TOKEN \
	--env COVERITY_SCAN_TOKEN=$COVERITY_SCAN_TOKEN \
	--env COVERITY_SCAN_NOTIFICATION_EMAIL=$COVERITY_SCAN_NOTIFICATION_EMAIL \
	--env FAULT_INJECTION=$FAULT_INJECTION \
	--env GITHUB_REPO=$GITHUB_REPO \
	--env CI_RUN=$CI_RUN \
	--env SRC_CHECKERS=$SRC_CHECKERS \
	--env BLACKLIST_FILE=$BLACKLIST_FILE \
	$ndctl_enable \
	--tmpfs /tmp:rw,relatime,suid,dev,exec,size=6G \
	-v $HOST_WORKDIR:$WORKDIR \
	-v /etc/localtime:/etc/localtime \
	-w $SCRIPTSDIR \
	$imageName $command
