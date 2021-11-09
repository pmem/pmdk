#!/usr/bin/env bash
#
# Copyright 2019-2020, Intel Corporation
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

set -e

DEV_DAX_R=0x0000
FS_DAX_R=0x0001
CONF_PATH[0]="/tmp/pmdk_config_0"
CONF_PATH[1]="/tmp/pmdk_config_1"
MOUNT_POINT[0]="/mnt/pmem0"
MOUNT_POINT[1]="/mnt/pmem1"
NONDEBUG_LIB_PATH=""

PMDK_CONVERT=false
PMDK_TEST=false
UNITTESTS=false
PKG=false
PYCHECK=false
FIO=false
DUAL=false
DAX_OFFSET=11

function usage()
{
	echo ""
	echo "Script for configuring namespaces, mountpoint, file permissions and generating config file for unittests and pmdk-tests."
	echo "Usage: $(basename $1) [-h|--help] [--conf-path_0=PATH] [--conf-path_1=PATH] [-u|--unittest] [-r|--pkg] [-c|--convert] [-t|--pmdk-test] [-p|--pycheck] [--conf-pmdk-nondebug-lib-path=NONDEBUG_LIB_PATH]"
	echo "-h, --help            Print help and exit"
	echo "-u, --unittest        Create config for unittests. Cannot be used with --pkg option."
	echo "-r, --pkg             Create config for pkg tests. Cannot be used with --unittest option."
	echo "-c, --convert         Create config for convert"
	echo "-t, --pmdk-test       Create config for pmdk-test"
	echo "-p, --pycheck         Create config for python unittest"
	echo "-d, --dual-ns         Create two sets of namespaces and config"
	echo "-f, --fio             Create fsdax and dax for fio test"
	echo "--conf-path_0=PATH    Path where to store testconfig.sh and config.xml config files. [default=${CONF_PATH[0]}]"
	echo "--conf-path_1=PATH    Path where to store testconfig.sh and config.xml config files. [default=${CONF_PATH[1]}]"
	echo "--conf-pmdk-nondebug-lib-path=NONDEBUG_LIB_PATH Path to installed pmdk libraries e.g. /usr/lib64"
}

function clear_namespaces() {
	scriptdir=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
	$scriptdir/removeNamespace.sh
}

function create_devdax() {
	local align=$1
	local size=$2
	local size_option="-s $size"

	if [ -z "$size" ]; then
		size_option=""
	fi

	local cmd="sudo ndctl create-namespace --mode devdax -a ${align} ${size_option} -r ${DEV_DAX_R} -f"
	result=$(${cmd})
	if [ $? -ne 0 ]; then
		exit 1;
	fi
	jq -r '.daxregion.devices[].chardev' <<< $result
}

function create_fsdax() {
	local size=$1

	local size_option="-s $size"

	if [ -z "$size" ]; then
		size_option=""
	fi

	local cmd="sudo ndctl create-namespace --mode fsdax ${size_option} -r ${FS_DAX_R} -f"
	result=$(${cmd})
	if [ $? -ne 0 ]; then
		exit 1;
	fi
	jq -r '.blockdev' <<< $result
}

#Namespaces size must align with interleave-width which is derived from number of dimms connected to a socked.
function check_alignment() {
	local size=$1
	local interleave_width=$(sudo ipmctl show -dimm -socket 1 | grep "0x1" | wc -l)
	local size_alignment=$(expr $size % $interleave_width)

	if [ "$size_alignment" -gt "0" ]; then
		size=$(expr $size - $size_alignment + $interleave_width)
	fi

	echo "${size}G"
}

#Namespaces for pmdk-convert test must be small enough otherwise tests will last forever.
function find_small_ns_size() {
	local size=$1
	#extract only digit part of size
	local size_digit=$(echo $size | tr -d -c 0-9)
	local size_in_mb=$(( $size_digit * 1024 ))
	
	while [ "$size_in_mb" -gt "1024" ]
	do
		size_in_mb=$(( $size_in_mb / 2))
	done

	echo "${size_in_mb}M"
}

while getopts ":cturdhpf-:" optchar; do
	case "${optchar}" in
		-)
		case "$OPTARG" in
			help) usage $0 && exit 0 ;;
			conf-path_0=*) CONF_PATH[0]="${OPTARG#*=}" ;;
			conf-path_1=*) CONF_PATH[1]="${OPTARG#*=}" ;;
			conf-pmdk-nondebug-lib-path=*) NONDEBUG_LIB_PATH="${OPTARG#*=}" ;;
			convert) PMDK_CONVERT=true ;;
			pmdk-test) PMDK_TEST=true ;;
			unittest) UNITTESTS=true ;;
			pkg) PKG=true ;;
			fio) FIO=true ;;
			pycheck) PYCHECK=true ;;
			dual-ns) DUAL=true ;;
			*) echo "Invalid argument '$OPTARG'"; usage $0 && exit 1 ;;
		esac
		;;
		c) PMDK_CONVERT=true ;;
		t) PMDK_TEST=true ;;
		u) UNITTESTS=true ;;
		r) PKG=true ;;
		p) PYCHECK=true ;;
		f) FIO=true ;;
		d) DUAL=true ;;
		h) usage $0 && exit 0 ;;
		*) echo "Invalid argument '$OPTARG'"; usage $0 && exit 1 ;;
	esac
done

#There is no default test cofiguration in this script. Configurations has to be specified.
if ! $PMDK_CONVERT && ! $PMDK_TEST && ! $UNITTESTS && ! $PKG && ! $PYCHECK && ! $FIO; then
	echo ""
	echo "ERROR: No config type selected. Please select one or more config types."
	exit 1
fi

#cannot mix unittests with pkg tests:
if $UNITTESTS && $PKG; then
	echo ""
	echo "ERROR: Cannot mix --unittests with --pkg options!"
	exit 1
fi

if $FIO && $DUAL; then
	echo ""
	echo "ERROR: Cannot mix fio and dual execution!"
	exit 1
fi

clear_namespaces

#fsdaxes are created on separete region than devdaxex and it has 256GB of storage.
FS_DAX_SIZE=$(check_alignment 100)
#Size of a namespace for the convert test.
CONVERT_NS_SIZE=$(check_alignment 1)
CONVERT_NS_SIZE=$(find_small_ns_size $CONVERT_NS_SIZE)

#If dual set of namespaces is selected, use smaller sizes of namaspaces to fit 256GB dimm.
if $DUAL; then
	n_conf=2
	BIG_NS_SIZE=$(check_alignment 25)
	SMALL_NS_SIZE=$(check_alignment 3)
else
	n_conf=1
	BIG_NS_SIZE=$(check_alignment 55)
	SMALL_NS_SIZE=$(check_alignment 4)
fi

#Creating devDax namespaces.
trap 'echo "ERROR: Failed to create namespaces"; clear_namespaces; exit 1' ERR SIGTERM SIGABRT

if $PMDK_CONVERT; then
	for (( i=0; i<$n_conf; i++ ))
	do
		dax[(9+$i*$DAX_OFFSET)]=$(create_devdax 4k $CONVERT_NS_SIZE)
		dax[(10+$i*$DAX_OFFSET)]=$(create_devdax 4k $CONVERT_NS_SIZE)
	done
fi

if $UNITTESTS || $PKG || $PYCHECK; then
	for (( i=0; i<$n_conf; i++ ))
	do
		dax[0+$i*$DAX_OFFSET]=$(create_devdax 4k $SMALL_NS_SIZE)
		dax[1+$i*$DAX_OFFSET]=$(create_devdax 4k $SMALL_NS_SIZE)
		dax[2+$i*$DAX_OFFSET]=$(create_devdax 2m $SMALL_NS_SIZE)
		dax[3+$i*$DAX_OFFSET]=$(create_devdax 2m $SMALL_NS_SIZE)
		dax[4+$i*$DAX_OFFSET]=$(create_devdax 4k $BIG_NS_SIZE)
		dax[5+$i*$DAX_OFFSET]=$(create_devdax 4k $BIG_NS_SIZE)
		dax[6+$i*$DAX_OFFSET]=$(create_devdax 2m $BIG_NS_SIZE)
		dax[7+$i*$DAX_OFFSET]=$(create_devdax 2m $BIG_NS_SIZE)
		dax[8+$i*$DAX_OFFSET]=$(create_devdax 2m $SMALL_NS_SIZE)
	done
fi

if $FIO; then
	create_devdax 2m
fi

#Creating mountpoint.
trap 'echo "ERROR: Failed to create mountpoint"; clear_namespaces; exit 1' ERR SIGTERM SIGABRT
if $UNITTESTS || $PKG || $PMDK_TEST || $PYCHECK || $FIO; then
	for (( i=0; i<$n_conf; i++ ))
	do
		if $FIO; then
			pmem_name[$i]=$(create_fsdax)
		else
			pmem_name[$i]=$(create_fsdax $FS_DAX_SIZE)
		fi

		if [ ! -d "${MOUNT_POINT[$i]}" ]; then
			sudo mkdir ${MOUNT_POINT[$i]}
		fi

		if ! grep -qs "${MOUNT_POINT[$i]} " /proc/mounts; then
			sudo mkfs.ext4 -F /dev/${pmem_name[$i]}
			sudo mount -o dax /dev/${pmem_name[$i]} ${MOUNT_POINT[$i]}
		fi
	done
fi

#Changing file permissions.
for (( i=0; i<$n_conf; i++ ))
do
	sudo chmod 777 ${MOUNT_POINT[$i]} || true
done

sudo chmod 777 /dev/dax* || true
sudo chmod a+rw /sys/bus/nd/devices/region*/deep_flush
sudo chmod +r /sys/bus/nd/devices/ndbus*/region*/resource
sudo chmod +r  /sys/bus/nd/devices/ndbus*/region*/dax*/resource

#Print created namespaces.
ndctl list -X | jq -r '.[] | select(.mode=="devdax") | [.daxregion.devices[].chardev, "align: "+(.align/1024|tostring+"k"), "size: "+(.size/1024/1024/1024|tostring+"G") ]'
ndctl list | jq -r '.[] | select(.mode=="fsdax") | [.blockdev, "size: "+(.size/1024/1024/1024|tostring+"G") ]'

#If config dir does not exists, create it
for (( i=0; i<$n_conf; i++ ))
do
	mkdir --parents ${CONF_PATH[$i]}
done

#Config file for unittests
if $UNITTESTS || $PKG; then
	for (( i=0; i<$n_conf; i++ ))
	do
		#Create config file for pmdk-unittests.
		cat >${CONF_PATH[$i]}/testconfig.sh <<EOL
# main & local
PMEM_FS_DIR=${MOUNT_POINT[$i]}
NON_PMEM_FS_DIR=/dev/shm/shm${i}
DEVICE_DAX_PATH=(/dev/${dax[0+$i*$DAX_OFFSET]} /dev/${dax[1+$i*$DAX_OFFSET]} /dev/${dax[2+$i*$DAX_OFFSET]} /dev/${dax[3+$i*$DAX_OFFSET]} /dev/${dax[8+$i*$DAX_OFFSET]} /dev/${dax[4+$i*$DAX_OFFSET]} /dev/${dax[5+$i*$DAX_OFFSET]} /dev/${dax[6+$i*$DAX_OFFSET]} /dev/${dax[7+$i*$DAX_OFFSET]})
KEEP_GOING=y
TM=1
UT_DUMP_LINES=1000

# remote
TEST_PROVIDERS=sockets
NODE[0]=127.0.0.1
NODE[1]=127.0.0.1
NODE[2]=127.0.0.1
NODE[3]=127.0.0.1
NODE_ADDR[0]=127.0.0.1
NODE_ADDR[1]=127.0.0.1
NODE_ADDR[2]=127.0.0.1
NODE_ADDR[3]=127.0.0.1
BASE_DIR=/dev/shm/shm${i}
NODE_WORKING_DIR[0]=\$BASE_DIR/dir0
NODE_WORKING_DIR[1]=\$BASE_DIR/dir1
NODE_WORKING_DIR[2]=\$BASE_DIR/dir2
NODE_WORKING_DIR[3]=\$BASE_DIR/dir3
NODE_LD_LIBRARY_PATH[0]="\${NODE_WORKING_DIR[0]}/debug:/usr/local/lib"
NODE_LD_LIBRARY_PATH[1]="\${NODE_WORKING_DIR[1]}/debug:/usr/local/lib"
NODE_LD_LIBRARY_PATH[2]="\${NODE_WORKING_DIR[2]}/debug:/usr/local/lib"
NODE_LD_LIBRARY_PATH[3]="\${NODE_WORKING_DIR[3]}/debug:/usr/local/lib"
NODE_DEVICE_DAX_PATH[0]="/dev/${dax[4+$i*$DAX_OFFSET]} /dev/${dax[5+$i*$DAX_OFFSET]} /dev/${dax[6+$i*$DAX_OFFSET]} /dev/${dax[7+$i*$DAX_OFFSET]}"
NODE_DEVICE_DAX_PATH[1]="/dev/${dax[0+$i*$DAX_OFFSET]} /dev/${dax[1+$i*$DAX_OFFSET]} /dev/${dax[2+$i*$DAX_OFFSET]} /dev/${dax[3+$i*$DAX_OFFSET]}"

# others
ENABLE_SUDO_TESTS=y
EOL

	if [ "$NONDEBUG_LIB_PATH" != "" ]
	then
		#Append variables to allow running the tests with prebuild binaries
		echo "PMDK_LIB_PATH_NONDEBUG=${NONDEBUG_LIB_PATH}" >> ${CONF_PATH[$i]}/testconfig.sh
		echo "TEST_BUILD=nondebug" >> ${CONF_PATH[$i]}/testconfig.sh
	fi

	if $PKG; then
		#Append variables exclusively for PKG tests:
		if grep -q "Fedora" /etc/*-release || grep -q "Red Hat Enterprise Linux" /etc/*-release; then
			echo "PMDK_LIB_PATH_NONDEBUG=/usr/lib64" >> ${CONF_PATH[$i]}/testconfig.sh
			echo "PMDK_LIB_PATH_DEBUG=/usr/lib64/pmdk_debug" >> ${CONF_PATH[$i]}/testconfig.sh
		elif grep -q "Ubuntu" /etc/*-release; then
			echo "PMDK_LIB_PATH_NONDEBUG=/lib/x86_64-linux-gnu" >> ${CONF_PATH[$i]}/testconfig.sh
			echo "PMDK_LIB_PATH_DEBUG=/lib/x86_64-linux-gnu/pmdk_dbg" >> ${CONF_PATH[$i]}/testconfig.sh
		fi
	fi
	done
fi

#Config file for pmdk-test
if $PMDK_TEST; then
	for (( i=0; i<$n_conf; i++ ))
	do
		#Create config file for pmdk-tests.
		cat >${CONF_PATH[$i]}/config.xml <<EOL
<configuration>
    <localConfiguration>
        <testDir>${MOUNT_POINT[$i]}</testDir>
    </localConfiguration>
</configuration>
EOL
	done
fi

#Config file for convert
if $PMDK_CONVERT; then
	for (( i=0; i<$n_conf; i++ ))
	do
		#Create config for pmdk convert tests: 2 small devdaxes:
		echo "/dev/${dax[9+$i*$DAX_OFFSET]} /dev/${dax[10+$i*$DAX_OFFSET]}" > "${CONF_PATH[$i]}/convertConfig.txt"
	done
fi

#Config file for pycheck
if $PYCHECK; then
	py_build_type[0]="debug"
	py_build_type[1]="release"
	for (( i=0; i<$n_conf; i++ ))
	do
		cat >${CONF_PATH[$i]}/testconfig.py <<EOL
config = {
    'unittest_log_level': 1,
#
# pmem_fs_dir, non_pmem_fs_dir and fs are deprecated but still used in pmdk 1.7.
#
    'pmem_fs_dir': '${MOUNT_POINT[$i]}',
    'non_pmem_fs_dir': '/tmp/${i}',
    'page_fs_dir': '/tmp/${i}',
    'fs': 'all',
    'cacheline_fs_dir': '${MOUNT_POINT[$i]}',
    'byte_fs_dir': '${MOUNT_POINT[$i]}',
    'force_cacheline': False,
    'force_page': False,
    'force_byte': True,
    'tm': True,
    'test_type': 'all',
    'build': '${py_build_type[$i]}',
    'granularity': 'all',
    'fail_on_skip': False,
    'keep_going': True,
    'timeout': '30m',
    'fs_dir_force_pmem': 0,
    'dump_lines': 30,
    'force_enable': None,
    'device_dax_path' : ['/dev/${dax[0+$i*$DAX_OFFSET]}', '/dev/${dax[1+$i*$DAX_OFFSET]}', '/dev/${dax[2+$i*$DAX_OFFSET]}', '/dev/${dax[3+$i*$DAX_OFFSET]}', '/dev/${dax[8+$i*$DAX_OFFSET]}', '/dev/${dax[4+$i*$DAX_OFFSET]}', '/dev/${dax[5+$i*$DAX_OFFSET]}', '/dev/${dax[6+$i*$DAX_OFFSET]}', '/dev/${dax[7+$i*$DAX_OFFSET]}'],
    'enable_admin_tests': True
}
EOL
	done
fi
