#!/usr/bin/env bash
#
# Copyright 2016-2019, Intel Corporation
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
# run-build-package.sh - is called inside a Docker container; prepares
#                        the environment and starts a build of PMDK project.
#

set -e

# Prepare build enviromnent
./prepare-for-build.sh

# Create fake tag, so that package has proper 'version' field
git config user.email "test@package.com"
git config user.name "test package"
git tag -a 1.4.99 -m "1.4" HEAD~1 || true

# Build all and run tests
cd $WORKDIR
export PCHECK_OPTS=-j2
make -j$(nproc) $PACKAGE_MANAGER

# Install packages
if [[ "$PACKAGE_MANAGER" == "dpkg" ]]; then
	cd $PACKAGE_MANAGER
	echo $USERPASS | sudo -S dpkg --install *.deb
else
	cd $PACKAGE_MANAGER/x86_64
	echo $USERPASS | sudo -S rpm --install *.rpm
fi

# Compile and run standalone test
cd $WORKDIR/utils/docker/test_package
make -j$(nproc) LIBPMEMOBJ_MIN_VERSION=1.4
./test_package testfile1

# Use pmreorder installed in the system
pmreorder_version="$(pmreorder -v)"
pmreorder_pattern="pmreorder\.py .+$"
(echo "$pmreorder_version" | grep -Ev "$pmreorder_pattern") && echo "pmreorder version failed" && exit 1

touch testfile2
touch logfile1
pmreorder -p testfile2 -l logfile1
