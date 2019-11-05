#!/usr/bin/env bash
#
# Copyright 2017-2019, Intel Corporation
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
# install-libndctl.sh - installs libndctl
#

set -e

OS=$2

echo "==== clone ndctl repo ===="
git clone https://github.com/pmem/ndctl.git
cd ndctl
git checkout $1

if [ "$OS" = "fedora" ]; then

echo "==== setup rpmbuild tree ===="
rpmdev-setuptree

RPMDIR=$HOME/rpmbuild/
VERSION=$(./git-version)
SPEC=./rhel/ndctl.spec

echo "==== create source tarball ====="
git archive --format=tar --prefix="ndctl-${VERSION}/" HEAD | gzip > "$RPMDIR/SOURCES/ndctl-${VERSION}.tar.gz"

echo "==== build ndctl ===="
./autogen.sh
./configure --disable-docs
make

echo "==== build ndctl packages ===="
rpmbuild -ba $SPEC

echo "==== install ndctl packages ===="
rpm -i $RPMDIR/RPMS/x86_64/*.rpm

echo "==== cleanup ===="
rm -rf $RPMDIR

else

echo "==== build ndctl ===="
./autogen.sh
./configure --disable-docs
make

echo "==== install ndctl ===="
make install

echo "==== cleanup ===="

fi

cd ..
rm -rf ndctl
