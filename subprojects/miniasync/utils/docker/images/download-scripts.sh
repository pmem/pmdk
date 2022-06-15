#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2021, Intel Corporation

#
# download-scripts.sh - downloads specific version of codecov's bash
#		script to generate and upload reports. It's useful,
#		since unverified version may break coverage results.
#

set -e

# master: Merge pull request #342 from codecov/revert-proj-name-..., 18.08.2020
CODECOV_VERSION="e877c1280cc6e902101fb5df2981ed1c962da7f0"

if [ "${SKIP_SCRIPTS_DOWNLOAD}" ]; then
	echo "Variable 'SKIP_SCRIPTS_DOWNLOAD' is set; skipping scripts' download"
	exit
fi

mkdir -p /opt/scripts

# Download codecov's bash script
git clone https://github.com/codecov/codecov-bash
cd codecov-bash
git checkout $CODECOV_VERSION

git apply ../0001-fix-generating-gcov-files-and-turn-off-verbose-log.patch
mv -v codecov /opt/scripts/codecov

cd ..
rm -rf codecov-bash
