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
# configure-tests.sh - is called inside a Docker container; configures tests
#                      and ssh server for use during build of PMDK project.
#

set -e

# Configure tests
cat << EOF > $WORKDIR/src/test/testconfig.sh
LONGDIR=LoremipsumdolorsitametconsecteturadipiscingelitVivamuslacinianibhattortordictumsollicitudinNullamvariusvestibulumligulaetegestaselitsemperidMaurisultriciesligulaeuipsumtinciduntluctusMorbimaximusvariusdolorid
# this path is ~3000 characters long
DIRSUFFIX="$LONGDIR/$LONGDIR/$LONGDIR/$LONGDIR/$LONGDIR"
NON_PMEM_FS_DIR=/tmp
PMEM_FS_DIR=/tmp
PMEM_FS_DIR_FORCE_PMEM=1
TEST_BUILD="debug nondebug"
ENABLE_SUDO_TESTS=y
TM=1
EOF

# Configure remote tests
if [[ $REMOTE_TESTS -eq 1 ]]; then
	echo "Configuring remote tests"
	cat << EOF >> $WORKDIR/src/test/testconfig.sh
NODE[0]=127.0.0.1
NODE_WORKING_DIR[0]=/tmp/node0
NODE_ADDR[0]=127.0.0.1
NODE_ENV[0]="PMEM_IS_PMEM_FORCE=1"
NODE[1]=127.0.0.1
NODE_WORKING_DIR[1]=/tmp/node1
NODE_ADDR[1]=127.0.0.1
NODE_ENV[1]="PMEM_IS_PMEM_FORCE=1"
NODE[2]=127.0.0.1
NODE_WORKING_DIR[2]=/tmp/node2
NODE_ADDR[2]=127.0.0.1
NODE_ENV[2]="PMEM_IS_PMEM_FORCE=1"
NODE[3]=127.0.0.1
NODE_WORKING_DIR[3]=/tmp/node3
NODE_ADDR[3]=127.0.0.1
NODE_ENV[3]="PMEM_IS_PMEM_FORCE=1"
TEST_BUILD="debug nondebug"
TEST_PROVIDERS=sockets
EOF

	mkdir -p ~/.ssh/cm

	cat << EOF >> ~/.ssh/config
Host 127.0.0.1
	StrictHostKeyChecking no
	ControlPath ~/.ssh/cm/%r@%h:%p
	ControlMaster auto
	ControlPersist 10m
EOF

	if [ ! -f /etc/ssh/ssh_host_rsa_key ]
	then
		(echo $USERPASS | sudo -S ssh-keygen -t rsa -C $USER@$HOSTNAME -P '' -f /etc/ssh/ssh_host_rsa_key)
	fi
	echo $USERPASS | sudo -S sh -c 'cat /etc/ssh/ssh_host_rsa_key.pub >> /etc/ssh/authorized_keys'
	ssh-keygen -t rsa -C $USER@$HOSTNAME -P '' -f ~/.ssh/id_rsa
	cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys
	chmod -R 700 ~/.ssh
	chmod 640 ~/.ssh/authorized_keys
	chmod 600 ~/.ssh/config

	# Start ssh service
	echo $USERPASS | sudo -S $START_SSH_COMMAND

	ssh 127.0.0.1 exit 0
else
	echo "Skipping remote tests"
	echo
	echo "Removing all libfabric.pc files in order to simulate that libfabric is not installed:"
	find /usr -name "libfabric.pc" 2>/dev/null
	echo $USERPASS | sudo -S sh -c 'find /usr -name "libfabric.pc" -exec rm -f {} + 2>/dev/null'
fi

# Configure python tests
	cat << EOF >> $WORKDIR/src/test/testconfig.py
config = {
	'unittest_log_level': 1,
	'pmem_fs_dir': '/tmp',
	'non_pmem_fs_dir': '/tmp',
	'tm': True,
	'test_type': 'check',
	'fs': 'all',
	'fs_dir_force_pmem': 0,
	'keep_going': False,
	'timeout': '3m',
	'build': ['debug', 'release'],
	'force_enable': None,
	'experimental': False,
   }
EOF

# Configure experimental tests
if [[ $EXPERIMENTAL -eq y ]]; then
	echo "Configuring experimental tests"
	cat << EOF >> $WORKDIR/src/test/testconfig.py
config = {
	'unittest_log_level': 1,
	'pmem_fs_dir': '/tmp',
	'non_pmem_fs_dir': '/tmp',
	'tm': True,
	'test_type': 'check',
	'fs': 'all',
	'fs_dir_force_pmem': 0,
	'keep_going': False,
	'timeout': '3m',
	'build': ['debug', 'release'],
	'force_enable': None,
	'experimental': True,
   }
EOF
fi
