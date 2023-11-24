#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2023, Intel Corporation

#
# run-coverity.sh - runs the Coverity scan build
#

set -e

if [[ "$CI_REPO_SLUG" != "$GITHUB_REPO" \
   && ( "$COVERITY_SCAN_NOTIFICATION_EMAIL" == "" \
     || "$COVERITY_SCAN_TOKEN" == "" ) ]]; then
	echo
	echo "Skipping Coverity build:"\
		"COVERITY_SCAN_TOKEN=\"$COVERITY_SCAN_TOKEN\" or"\
		"COVERITY_SCAN_NOTIFICATION_EMAIL="\
		"\"$COVERITY_SCAN_NOTIFICATION_EMAIL\" is not set"
	exit 0
fi

# Prepare build environment
./prepare-for-build.sh

CERT_FILE=/etc/ssl/certs/ca-certificates.crt
TEMP_CF=$(mktemp)
cp $CERT_FILE $TEMP_CF

# Download Coverity certificate
echo -n | openssl s_client -connect scan.coverity.com:443 | \
	sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' | \
	tee -a $TEMP_CF

echo $USERPASS | sudo -S mv $TEMP_CF $CERT_FILE

export COVERITY_SCAN_PROJECT_NAME="$CI_REPO_SLUG"
if [[ "$CI_EVENT_TYPE" == "cron" || "$CI_EVENT_TYPE" == "workflow_dispatch" ]]; then
	export COVERITY_SCAN_BRANCH_PATTERN="master"
else
	export COVERITY_SCAN_BRANCH_PATTERN="coverity_scan"
fi
export COVERITY_SCAN_BUILD_COMMAND="make -j$(nproc) all"

cd $WORKDIR

#
# Run the Coverity scan
#

# The 'travisci_build_coverity_scan.sh' script requires the following
# environment variables to be set:
# - TRAVIS_BRANCH - has to contain the name of the current branch
# - TRAVIS_PULL_REQUEST - has to be set to 'true' in case of pull requests
#
export TRAVIS_BRANCH=${CI_BRANCH}
[ "${CI_EVENT_TYPE}" == "pull_request" ] && export TRAVIS_PULL_REQUEST="true"

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

COVERITY_LOG_FILE=/pmdk/cov-int/scm_log.txt
if [ -f $COVERITY_LOG_FILE ]; then
	echo "Coverity log file detected: $COVERITY_LOG_FILE"
	cat $COVERITY_LOG_FILE
else
	echo "Coverity log not found ($COVERITY_LOG_FILE)"
fi
