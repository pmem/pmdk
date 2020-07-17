#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2020, Intel Corporation

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
cd $WORKDIR
make -j$(nproc) COVERAGE=1
make -j$(nproc) test COVERAGE=1

# XXX: unfortunately valgrind raports issues in coverage instrumentation
# which we have to ignore (-k flag), also there is dependency between
# local and remote tests (which cannot be easily removed) we have to
# run local and remote tests separately
cd src/test
# do not change -j2 to -j$(nproc) in case of tests (make check/pycheck)
make -kj2 pcheck-local-quiet TEST_BUILD=debug || true
make check-remote-quiet TEST_BUILD=debug || true
# do not change -j2 to -j$(nproc) in case of tests (make check/pycheck)
make -j2 pycheck TEST_BUILD=debug || true
cd ../..

# prepare flag for codecov report to differentiate builds
flag=tests
[ -n "$GITHUB_ACTIONS" ] && flag=GHA
[ -n "$TRAVIS" ] && flag=Travis

# run gcov exe, using codecov's bash (remove parsed coverage files, set flag and exit 1 if not successful)
/opt/scripts/codecov -c -F ${flag} -Z

printf "check for any leftover gcov files\n"
leftover_files=$(find . -name "*.gcov" | wc -l)
if [[ $leftover_files > 0 ]]; then
	# display found files and exit with error (they all should be parsed)
	find . -name "*.gcov"
	return 1
fi
