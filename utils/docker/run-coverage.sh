#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2024, Intel Corporation

#
# run-coverage.sh - is called inside a Docker container; runs tests
#                   to measure code coverage and sends report to codecov.io
#

set -e

# Get and prepare PMDK source
./prepare-for-build.sh

# Hush error messages, mainly from Valgrind
export UT_DUMP_LINES=0

# Skip printing mismatched files for tests with Valgrind
export UT_VALGRIND_SKIP_PRINT_MISMATCHED=1

# Build all and run tests
pushd ${WORKDIR}
make -j$(nproc) COVERAGE=1
make -j$(nproc) test COVERAGE=1

# XXX: unfortunately valgrind reports issues in coverage instrumentation
# which we have to ignore (-k flag)
pushd src/test
# do not change -j2 to -j$(nproc) in case of tests (make check/pycheck)
make -kj2 pcheck-local-quiet TEST_BUILD=debug || true
# do not change -j2 to -j$(nproc) in case of tests (make check/pycheck)
make -kj2 pycheck TEST_BUILD=debug || true
popd
