#!/usr/bin/env bash
#
# Copyright 2017-2018, Intel Corporation
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
# run-coverage.sh - is called inside a Docker container; runs the coverage
#                   test
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
make -j2 USE_LIBUNWIND=1 COVERAGE=1
make -j2 test USE_LIBUNWIND=1 COVERAGE=1

# XXX: unfortunately valgrind raports issues in coverage instrumentation
# which we have to ignore (-k flag), also there is dependency between
# local and remote tests (which cannot be easily removed) we have to
# run local and remote tests separately
cd src/test
make -kj2 pcheck-local-quiet TEST_BUILD=debug || true
make check-remote-quiet TEST_BUILD=debug || true
cd ../..
bash <(curl -s https://codecov.io/bash)
