#!/bin/bash -e
#
# Copyright 2016, Intel Corporation
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
# .travis_config.sh -- travis configuration script
#

do_remote=0
do_libfabric=0
do_force=0
libfabric_ver="8e21233251fad4868b2209de473fe1ed585ec564"
libfabric_url="https://github.com/ofiwg/libfabric/archive/"



for arg in "$@"; do
	shift
	case "$arg" in
	"--remote")
		do_remote=1
		;;
	"--libfabric")
		do_libfabric=1
		;;
	"--force")
		do_force=1
		;;
	esac
done

if [ "$TRAVIS" != "true" ]; then
	if [ "$do_force" != "1" ]; then
		echo "Trying to run configuration script not on travis."
		echo "If you want to run this script anyway use --force option"
		exit 0
	fi
fi

cat << EOF > src/test/testconfig.sh
NON_PMEM_FS_DIR=/tmp
PMEM_FS_DIR=/tmp
PMEM_FS_DIR_FORCE_PMEM=1
EOF

if [ "$do_remote" == "1" ]; then
	echo "Configuring remote tests"
	cat << EOF >> src/test/testconfig.sh
NODE[0]=127.0.0.1
NODE_WORKING_DIR[0]=/tmp/node0
NODE_ADDR[0]=127.0.0.1
NODE[1]=127.0.0.1
NODE_WORKING_DIR[1]=/tmp/node1
NODE_ADDR[1]=127.0.0.1
EOF

	cat << EOF >> ~/.ssh/config
Host 127.0.0.1
	StrictHostKeyChecking no
	ControlPath ~/.ssh/cm/%r@%h:%p
	ControlMaster auto
	ControlPersist 10m
EOF

	mkdir -p ~/.ssh/cm/
	ssh-keygen -t rsa -C $USER@$HOSTNAME -P '' -f ~/.ssh/id_rsa
	cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys
	ssh 127.0.0.1 exit 0
else
	echo "Skipping remote tests"
fi

if [ "$do_libfabric" == "1" ]; then
	echo "Configuring libfabric"
	pushd ~
	libfabric_dir="libfabric-${libfabric_ver}"
	libfabric_tarball="${libfabric_ver}.zip"
	wget "${libfabric_url}/${libfabric_tarball}"
	unzip $libfabric_tarball
	pushd $libfabric_dir
	./autogen.sh
	./configure --prefix=/usr --enable-sockets
	make -j2
	sudo make install
	popd
	popd
else
	echo "Skipping libfabric"
fi

