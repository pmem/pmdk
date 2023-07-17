#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2023, Intel Corporation

#
# prepare-for-build.sh - is called inside a Docker container; prepares
#                        the environment inside a Docker container for
#                        running build of PMDK project.
#

set -e

# This should be run only on CIs
if [ "$CI_RUN" == "YES" ]; then
	# Make sure $WORKDIR has correct access rights
	# - set them to the current UID and GID
	echo $USERPASS | sudo -S chown -R $(id -u).$(id -g) $WORKDIR
fi

# Configure tests (e.g. python tests) unless the current configuration
# should be preserved
KEEP_TEST_CONFIG=${KEEP_TEST_CONFIG:-0}
if [[ "$KEEP_TEST_CONFIG" == 0 ]]; then
	OUTPUT_DIR=$WORKDIR/src/test \
		PMEM_FS_DIR=/dev/shm \
		PMEM_FS_DIR_FORCE_PMEM=1 \
		force_cacheline=True \
		force_byte=True \
		../create-testconfig.sh
fi
