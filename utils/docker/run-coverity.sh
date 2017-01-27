#!/bin/bash -e
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
# run-coverity.sh - runs the Coverity scan build
#

# Prepare build environment
./prepare-for-build.sh

# Download Coverity certificate
echo -n | openssl s_client -connect scan.coverity.com:443 | \
	sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' | \
	sudo tee -a /etc/ssl/certs/ca-;

# Build librpmem even if libfabric is not compiled with ibverbs
export RPMEM_DISABLE_LIBIBVERBS=y

export COVERITY_SCAN_PROJECT_NAME="$TRAVIS_REPO_SLUG"
[[ "$TRAVIS_EVENT_TYPE" == "cron" ]] \
	&& export COVERITY_SCAN_BRANCH_PATTERN="master" \
	|| export COVERITY_SCAN_BRANCH_PATTERN="coverity_scan"
export COVERITY_SCAN_BUILD_COMMAND="make -j all"

cd $WORKDIR

# Run the Coverity scan
curl -s https://scan.coverity.com/scripts/travisci_build_coverity_scan.sh | bash
