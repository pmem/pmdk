#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation

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
	find /usr -name "libfabric.pc" 2>/dev/null || true
	echo $USERPASS | sudo -S sh -c 'find /usr -name "libfabric.pc" -exec rm -f {} + 2>/dev/null'
fi

# Configure python tests
	cat << EOF >> $WORKDIR/src/test/testconfig.py
config = {
	'unittest_log_level': 1,
	'cacheline_fs_dir': '/tmp',
	'force_cacheline': True,
	'page_fs_dir': '/tmp',
	'force_page': False,
	'byte_fs_dir': '/tmp',
	'force_byte': True,
	'tm': True,
	'test_type': 'check',
	'granularity': 'all',
	'fs_dir_force_pmem': 0,
	'keep_going': False,
	'timeout': '3m',
	'build': ['debug', 'release'],
	'force_enable': None,
	'device_dax_path': [],
	'fail_on_skip': False,
	'allow_using_sudo': True
   }
EOF

