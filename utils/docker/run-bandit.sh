#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

#
# run-bandit.sh - is called inside a Docker container; runs bandit
# security checker for code written in python
#

set -e

# Get and prepare PMDK source
./prepare-for-build.sh

cd $WORKDIR

# set path to pmreorder tool
# at the moment pmreorder is the only python tool
# released in the PMDK
SCAN_DIR=src/tools/pmreorder

echo "Start Bandit scan"

bandit --version
bandit -r "$SCAN_DIR"

echo "End Bandit scan"
