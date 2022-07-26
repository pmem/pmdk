#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2022, Intel Corporation

#
# run-build.sh - is called inside a Docker container; prepares the environment
#                and starts a build of PMDK project.
#

set -e

# Prepare build environment
./prepare-for-build.sh

# Build all and run tests
cd $WORKDIR
if [ "$SRC_CHECKERS" != "0" ]; then
	make -j$(nproc) check-license
	make -j$(nproc) cstyle
fi

echo "## Running make"
make -j$(nproc)
echo ""
echo "## Running make test"
make -j$(nproc) test
echo ""
echo "## Running make pcheck"
# do not change -j2 to -j$(nproc) in case of tests (make check/pycheck)
make -j2 pcheck TEST_BUILD=$TEST_BUILD
echo ""
echo "## Running make pycheck"
# do not change -j2 to -j$(nproc) in case of tests (make check/pycheck)
make -j2 pycheck
echo ""
echo "## Running make source"
make -j$(nproc) DESTDIR=/tmp source

# Create PR with generated docs
if [ "${AUTO_DOC_UPDATE}" == "1" ]; then
	echo ""
	echo "## Running auto doc update"
	./utils/docker/run-doc-update.sh
fi
