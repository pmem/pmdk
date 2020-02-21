#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2020, Intel Corporation

#
# run-coverity.sh - runs the Coverity scan build
#

set -e

# Prepare build environment
./prepare-for-build.sh

# Download Coverity certificate
echo -n | openssl s_client -connect scan.coverity.com:443 | \
	sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' | \
	sudo tee -a /etc/ssl/certs/ca-;

export COVERITY_SCAN_PROJECT_NAME="$CI_REPO_SLUG"
[[ "$CI_EVENT_TYPE" == "cron" ]] \
	&& export COVERITY_SCAN_BRANCH_PATTERN="master" \
	|| export COVERITY_SCAN_BRANCH_PATTERN="coverity_scan"
export COVERITY_SCAN_BUILD_COMMAND="make -j$(nproc) all"

cd $WORKDIR

# Run the Coverity scan

# XXX: Patch the Coverity script.
# Recently, this script regularly exits with an error, even though
# the build is successfully submitted.  Probably because the status code
# is missing in response, or it's not 201.
# Changes:
# 1) change the expected status code to 200 and
# 2) print the full response string.
#
# This change should be reverted when the Coverity script is fixed.
#
# The previous version was:
# curl -s https://scan.coverity.com/scripts/travisci_build_coverity_scan.sh | bash

wget https://scan.coverity.com/scripts/travisci_build_coverity_scan.sh
patch < utils/docker/0001-travis-fix-travisci_build_coverity_scan.sh.patch
bash ./travisci_build_coverity_scan.sh
