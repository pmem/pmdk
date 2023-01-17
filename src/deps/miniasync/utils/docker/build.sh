#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2023, Intel Corporation
#

#
# build.sh - runs a Docker container from a Docker image with environment
#            prepared for running miniasync build and tests.
#
#
# Notes:
# - run this script from its location or set the variable 'HOST_WORKDIR' to
#   where the root of this project is on the host machine,
# - set variables 'OS' and 'OS_VER' properly to a system you want to build this
#   repo on (for proper values take a look on the list of Dockerfiles at the
#   utils/docker/images directory), eg. OS=ubuntu, OS_VER=20.04.
#

set -e

source $(dirname $0)/set-ci-vars.sh
source $(dirname $0)/valid-branches.sh

if [[ -z "$OS" || -z "$OS_VER" ]]; then
	echo "ERROR: The variables OS and OS_VER have to be set " \
		"(eg. OS=fedora, OS_VER=37)."
	exit 1
fi

if [[ -z "$HOST_WORKDIR" ]]; then
	HOST_WORKDIR=$(readlink -f ../..)
fi

if [[ "$TYPE" == "coverity" && "$CI_EVENT_TYPE" != "cron" && "$CI_BRANCH" != "coverity_scan" ]]; then
	echo "Skipping Coverity job for non cron/Coverity build"
	exit 0
fi

if [[ "$CI_BRANCH" == "coverity_scan" && "$TYPE" != "coverity" ]]; then
	echo "Skipping non-Coverity job for cron/Coverity build"
	exit 0
fi

imageName=${DOCKER_REPO}:${IMG_VER}-${OS}-${OS_VER}
containerName=miniasync-${OS}-${OS_VER}

if [[ "$command" == "" ]]; then
	case $TYPE in
		normal)
			command="./run-build.sh";
			;;
		coverity)
			command="./run-coverity.sh";
			;;
	esac
fi

if [ "$COVERAGE" == "1" ]; then
	docker_opts="${docker_opts} `bash <(curl -s https://codecov.io/env)`";
fi

if [ -n "$DNS_SERVER" ]; then DNS_SETTING=" --dns=$DNS_SERVER "; fi

# Only run doc update on $GITHUB_REPO master or stable branch
if [[ -z "${CI_BRANCH}" || -z "${TARGET_BRANCHES[${CI_BRANCH}]}" || "$CI_EVENT_TYPE" == "pull_request" || "$CI_REPO_SLUG" != "${GITHUB_REPO}" ]]; then
	AUTO_DOC_UPDATE=0
fi

WORKDIR=/miniasync
SCRIPTSDIR=$WORKDIR/utils/docker

# check if we are running on a CI (Travis or GitHub Actions)
[ -n "$GITHUB_ACTIONS" -o -n "$TRAVIS" ] && CI_RUN="YES" || CI_RUN="NO"

# do not allocate a pseudo-TTY if we are running on GitHub Actions
[ ! $GITHUB_ACTIONS ] && TTY='-t' || TTY=''

echo Building ${IMG_VER}-${OS}-${OS_VER}

# Run a container with
#  - environment variables set (--env)
#  - host directory containing source mounted (-v)
#  - working directory set (-w)
docker run --privileged=true --name=$containerName -i $TTY \
	$DNS_SETTING \
	${docker_opts} \
	--env http_proxy=$http_proxy \
	--env https_proxy=$https_proxy \
	--env AUTO_DOC_UPDATE=$AUTO_DOC_UPDATE \
	--env GITHUB_ACTIONS=$GITHUB_ACTIONS \
	--env GITHUB_HEAD_REF=$GITHUB_HEAD_REF \
	--env GITHUB_REPO=$GITHUB_REPO \
	--env GITHUB_REPOSITORY=$GITHUB_REPOSITORY \
	--env GITHUB_REF=$GITHUB_REF \
	--env GITHUB_RUN_ID=$GITHUB_RUN_ID \
	--env GITHUB_SHA=$GITHUB_SHA \
	--env WORKDIR=$WORKDIR \
	--env SCRIPTSDIR=$SCRIPTSDIR \
	--env COVERAGE=$COVERAGE \
	--env CI_COMMIT=$CI_COMMIT \
	--env CI_COMMIT_RANGE=$CI_COMMIT_RANGE \
	--env CI_REPO_SLUG=$CI_REPO_SLUG \
	--env CI_BRANCH=$CI_BRANCH \
	--env CI_EVENT_TYPE=$CI_EVENT_TYPE \
	--env CI_RUN=$CI_RUN \
	--env CI_SANITS=$CI_SANITS \
	--env TRAVIS=$TRAVIS \
	--env COVERITY_SCAN_TOKEN=$COVERITY_SCAN_TOKEN \
	--env COVERITY_SCAN_NOTIFICATION_EMAIL=$COVERITY_SCAN_NOTIFICATION_EMAIL \
	--env DOC_UPDATE_GITHUB_TOKEN=$DOC_UPDATE_GITHUB_TOKEN \
	--env TEST_BUILD=$TEST_BUILD \
	--env DEFAULT_TEST_DIR=/dev/shm \
	--env TEST_PACKAGES=${TEST_PACKAGES:-ON} \
	--env CHECK_CSTYLE=${CHECK_CSTYLE:-ON} \
	--env FAULT_INJECTION=$FAULT_INJECTION \
	--env CC=${CC:-gcc} \
	--shm-size=4G \
	-v $HOST_WORKDIR:$WORKDIR \
	-v /etc/localtime:/etc/localtime \
	-w $SCRIPTSDIR \
	$imageName $command
