#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# download-scripts.sh - downloads specific version of codecov's bash
#		script to generate and upload reports. It's useful,
#		since unverified version may break coverage results.
#

set -e

# master: Merge pull request #331 from codecov/update-env, 07.07.2020
CODECOV_VERSION="353aa93e4036da8b1566c8d4dbfee1e51336dc5d"

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
