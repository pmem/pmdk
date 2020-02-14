#!/usr/bin/env bash
#
# Copyright 2016-2020, Intel Corporation
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
# prepare-for-build.sh - is called inside a Docker container; prepares
#                        the environment inside a Docker container for
#                        running build of PMDK project.
#

set -e

# Mount filesystem for tests
echo $USERPASS | sudo -S mount -t tmpfs none /tmp -osize=6G

# This should be run only on CIs
if [ "$CI_RUN" == "YES" ]; then
	# Make sure $WORKDIR has correct access rights
	# - set them to the current UID and GID
	echo $USERPASS | sudo -S chown -R $(id -u).$(id -g) $WORKDIR
fi

# Configure tests (e.g. ssh for remote tests) unless the current configuration
# should be preserved
KEEP_TEST_CONFIG=${KEEP_TEST_CONFIG:-0}
if [[ "$KEEP_TEST_CONFIG" == 0 ]]; then
	./configure-tests.sh
fi

# Check for changes in automatically generated docs (only when on Travis)
if [[ -n "$TRAVIS" ]]; then
	../check-doc.sh
fi
