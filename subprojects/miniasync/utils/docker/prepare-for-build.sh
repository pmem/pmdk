#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

#
# prepare-for-build.sh - prepare the Docker image for the build
#

set -e

function sudo_password() {
	echo $USERPASS | sudo -Sk $*
}

# this should be run only on CIs
if [ "$CI_RUN" == "YES" ]; then
	sudo_password chown -R $(id -u).$(id -g) $WORKDIR
fi
