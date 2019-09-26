#!/usr/bin/env bash
#
# Copyright 2018-2019, Intel Corporation
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

#
# src/test/common_badblock.sh -- commons for the following tests:
#                                - util_badblock
#                                - pmempool_create
#                                - pmempool_info
#

LOG=out${UNITTEST_NUM}.log

UNITTEST_DIRNAME=$(echo $UNITTEST_NAME | cut -d'/' -f1)

COMMAND_MOUNTED_DIRS="\
	mount | grep -e $UNITTEST_DIRNAME | cut -d' ' -f1 | xargs && true"

COMMAND_NDCTL_NFIT_TEST_INIT="\
	sudo modprobe nfit_test &>>$PREP_LOG_FILE && \
	sudo ndctl disable-region all &>>$PREP_LOG_FILE && \
	sudo ndctl zero-labels all &>>$PREP_LOG_FILE && \
	sudo ndctl enable-region all &>>$PREP_LOG_FILE"

COMMAND_NDCTL_NFIT_TEST_FINI="\
	sudo ndctl disable-region all &>>$PREP_LOG_FILE && \
	sudo modprobe -r nfit_test &>>$PREP_LOG_FILE"


#
# badblock_test_init -- initialize badblock test based on underlying hardware
#
# Input arguments:
# 1) device type (dax_device|block_device)
# 2) mount directory (in case of block device type)
#
function badblock_test_init() {
	case "$1"
	in
	dax_device|block_device)
		;;
	*)
		usage "bad device type: $1"
		;;
	esac
	DEVTYPE=$1

	if [ "$BADBLOCK_TEST_TYPE" == "nfit_test" ]; then
		ndctl_nfit_test_init
	fi

	if [ "$DEVTYPE" == "dax_device" ]; then
		DEVICE=$(badblock_test_get_dax_device)
	elif [ "$DEVTYPE" == "block_device" ]; then
		DEVICE=$(badblock_test_get_block_device)
		prepare_mount_dir $DEVICE $2
	fi
	NAMESPACE=$(ndctl_get_namespace_of_device $DEVICE)
	FULLDEV="/dev/$DEVICE"
}

#
# badblock_test_init_node -- initialize badblock test based on underlying
# hardware on a remote node
#
# Input arguments:
# 1) remote node number
# 2) device type (dax_device|block_device)
# 3) for block device: mount directory
#    for dax device on real pmem: dax device index on a given node
#
function badblock_test_init_node() {
	case "$2"
	in
	dax_device|block_device)
		;;
	*)
		usage "bad device type: $2"
		;;
	esac
	DEVTYPE=$2

	if [ "$BADBLOCK_TEST_TYPE" == "nfit_test" ]; then
		ndctl_nfit_test_init_node $1
	fi

	if [ "$DEVTYPE" == "dax_device" ]; then
		DEVICE=$(badblock_test_get_dax_device_node $1 $3)
	elif [ "$DEVTYPE" == "block_device" ]; then
		DEVICE=$(badblock_test_get_block_device_node $1)
		prepare_mount_dir_node $1 $DEVICE $3
	fi
	NAMESPACE=$(ndctl_get_namespace_of_device_node $1 $DEVICE)
	FULLDEV="/dev/$DEVICE"
}

#
# badblock_test_get_dax_device -- get name of the dax device
#
function badblock_test_get_dax_device() {
	DEVICE=""
	if [ "$BADBLOCK_TEST_TYPE" == "nfit_test" ]; then
		DEVICE=$(ndctl_nfit_test_get_dax_device)
	elif [ "$BADBLOCK_TEST_TYPE" == "real_pmem" ]; then
		DEVICE=$(real_pmem_get_dax_device)
	fi
	echo $DEVICE
}

#
# badblock_test_get_dax_device_node -- get name of the dax device on a given
#                                      remote node
# Input arguments:
# 1) remote node number
# 2) For real pmem: device dax index on a given node
#
function badblock_test_get_dax_device_node() {
	DEVICE=""
	if [ "$BADBLOCK_TEST_TYPE" == "nfit_test" ]; then
		DEVICE=$(ndctl_nfit_test_get_dax_device_node $1)
	elif [ "$BADBLOCK_TEST_TYPE" == "real_pmem" ]; then
		DEVICE=$(real_pmem_get_dax_device_node $1 $2)
	fi
	echo $DEVICE
}

#
# badblock_test_get_block_device -- get name of the block device
#
function badblock_test_get_block_device() {
	DEVICE=""
	if [ "$BADBLOCK_TEST_TYPE" == "nfit_test" ]; then
		DEVICE=$(ndctl_nfit_test_get_block_device)
	elif [ "$BADBLOCK_TEST_TYPE" == "real_pmem" ]; then
		DEVICE=$(real_pmem_get_block_device)
	fi
	echo "$DEVICE"
}

#
# badblock_test_get_block_device_node -- get name of the block device on a given
#                                        remote node
#
function badblock_test_get_block_device_node() {
	DEVICE=""
	if [ "$BADBLOCK_TEST_TYPE" == "nfit_test" ]; then
		DEVICE=$(ndctl_nfit_test_get_block_device_node $1)
	elif [ "$BADBLOCK_TEST_TYPE" == "real_pmem" ]; then
		DEVICE=$(real_pmem_get_block_device_node $1)
	fi
	echo "$DEVICE"
}

#
# prepare_mount_dir -- prepare the mount directory for provided device
#
# Input arguments:
# 1) device name
# 2) mount directory
#
function prepare_mount_dir() {
	if [ "$BADBLOCK_TEST_TYPE" == "nfit_test" ]; then
		local FULLDEV="/dev/$1"
		ndctl_nfit_test_mount_pmem $FULLDEV $2
	elif [ "$BADBLOCK_TEST_TYPE" == "real_pmem" ]; then
		if [ ! -d $2 ]; then
			mkdir -p $2
		fi
	fi
}

#
# prepare_mount_dir_node -- prepare the mount directory for provided device
#                           on a given remote node
#
# Input arguments:
# 1) remote node number
# 2) device name
# 3) mount directory
#
function prepare_mount_dir_node() {
	if [ "$BADBLOCK_TEST_TYPE" == "nfit_test" ]; then
		local FULLDEV="/dev/$2"
		ndctl_nfit_test_mount_pmem_node $1 $FULLDEV $3
	elif [ "$BADBLOCK_TEST_TYPE" == "real_pmem" ]; then
		if [ ! -d $3 ]; then
			run_on_node $1 "mkdir -p $3"
		fi
	fi
}


#
# real_pmem_get_dax_device -- get real pmem dax device name
#
function real_pmem_get_dax_device() {
	local FULLDEV=${DEVICE_DAX_PATH[0]}
	DEVICE=${FULLDEV##*/}
	echo $DEVICE
}

#
# real_pmem_get_dax_device_node -- get real pmem dax device name on a given
#                                  remote node
#
# Input arguments:
# 1) remote node number
# 2) device dax index number
#
function real_pmem_get_dax_device_node() {
	local node=$1
	local devdax_index=$2

	local device_dax_path=(${NODE_DEVICE_DAX_PATH[$node]})

	local FULLDEV=${device_dax_path[$devdax_index]}

	DEVICE=${FULLDEV##*/}
	echo $DEVICE
}

#
# real_pmem_get_block_device -- get real pmem block device name
#
function real_pmem_get_block_device() {
	local FULL_DEV=$(mount | grep $PMEM_FS_DIR | cut -f 1 -d" ")
	DEVICE=${FULL_DEV##*/}
	echo $DEVICE
}

#
# real_pmem_get_block_device_node -- get real pmem block device name on a given
#                                    remote node
#
function real_pmem_get_block_device_node() {
	local FULL_DEV=$(expect_normal_exit run_on_node $1 mount | grep $PMEM_FS_DIR | cut -f 1 -d" ")
	DEVICE=${FULL_DEV##*/}
	echo $DEVICE
}

#
# ndctl_nfit_test_init -- reset all regions and reload the nfit_test module
#
function ndctl_nfit_test_init() {
	sudo ndctl disable-region all &>>$PREP_LOG_FILE
	if ! sudo modprobe -r nfit_test &>>$PREP_LOG_FILE; then
		MOUNTED_DIRS="$(eval $COMMAND_MOUNTED_DIRS)"
		[ "$MOUNTED_DIRS" ] && sudo umount $MOUNTED_DIRS
		sudo ndctl disable-region all &>>$PREP_LOG_FILE
		sudo modprobe -r nfit_test
	fi
	expect_normal_exit $COMMAND_NDCTL_NFIT_TEST_INIT
}

#
# ndctl_nfit_test_init_node -- reset all regions and reload the nfit_test
#                              module on a remote node
#
function ndctl_nfit_test_init_node() {
	run_on_node $1 "sudo ndctl disable-region all &>>$PREP_LOG_FILE"
	if ! run_on_node $1 "sudo modprobe -r nfit_test &>>$PREP_LOG_FILE"; then
		MOUNTED_DIRS="$(run_on_node $1 "$COMMAND_MOUNTED_DIRS")"
		run_on_node $1 "\
			[ \"$MOUNTED_DIRS\" ] && sudo umount $MOUNTED_DIRS; \
			sudo ndctl disable-region all &>>$PREP_LOG_FILE; \
			sudo modprobe -r nfit_test"
	fi
	expect_normal_exit run_on_node $1 "$COMMAND_NDCTL_NFIT_TEST_INIT"
}

#
# badblock_test_fini -- clean badblock test based on underlying hardware
#
# Input arguments:
# 1) pmem mount directory to be umounted (optional)
#
function badblock_test_fini() {
	if [ "$BADBLOCK_TEST_TYPE" == "nfit_test" ]; then
		ndctl_nfit_test_fini $1
	fi
}

#
# badblock_test_fini_node() -- clean badblock test based on underlying hardware
#                              on a given remote node
#
# Input arguments:
# 1) node number
# 2) pmem mount directory to be umounted (optional)
#
function badblock_test_fini_node() {
	if [ "$BADBLOCK_TEST_TYPE" == "nfit_test" ]; then
		ndctl_nfit_test_fini_node $1 $2
	fi
}

#
# ndctl_nfit_test_fini -- clean badblock test ran on nfit_test based on underlying hardware
#
function ndctl_nfit_test_fini() {
       MOUNT_DIR=$1
       [ $MOUNT_DIR ] && sudo umount $MOUNT_DIR &>> $PREP_LOG_FILE
       expect_normal_exit $COMMAND_NDCTL_NFIT_TEST_FINI
}

#
# ndctl_nfit_test_fini_node -- disable all regions, remove the nfit_test module
#                              and (optionally) umount the pmem block device on a remote node
#
# Input arguments:
# 1) node number
# 2) pmem mount directory to be umounted
#
function ndctl_nfit_test_fini_node() {
	MOUNT_DIR=$2
	[ $MOUNT_DIR ] && expect_normal_exit run_on_node $1 "sudo umount $MOUNT_DIR &>> $PREP_LOG_FILE"
	expect_normal_exit run_on_node $1 "$COMMAND_NDCTL_NFIT_TEST_FINI"
}

#
# ndctl_nfit_test_mount_pmem -- mount a pmem block device
#
# Input arguments:
# 1) path of a pmem block device
# 2) mount directory
#
function ndctl_nfit_test_mount_pmem() {
	FULLDEV=$1
	MOUNT_DIR=$2
	expect_normal_exit "\
		sudo mkfs.ext4 $FULLDEV &>>$PREP_LOG_FILE && \
		sudo mkdir -p $MOUNT_DIR &>>$PREP_LOG_FILE && \
		sudo mount $FULLDEV $MOUNT_DIR &>>$PREP_LOG_FILE && \
		sudo chmod 0777 $MOUNT_DIR"
}

#
# ndctl_nfit_test_mount_pmem_node -- mount a pmem block device on a remote node
#
# Input arguments:
# 1) number of a node
# 2) path of a pmem block device
# 3) mount directory
#
function ndctl_nfit_test_mount_pmem_node() {
	FULLDEV=$2
	MOUNT_DIR=$3
	expect_normal_exit run_on_node $1 "\
		sudo mkfs.ext4 $FULLDEV &>>$PREP_LOG_FILE && \
		sudo mkdir -p $MOUNT_DIR &>>$PREP_LOG_FILE && \
		sudo mount $FULLDEV $MOUNT_DIR &>>$PREP_LOG_FILE && \
		sudo chmod 0777 $MOUNT_DIR"
}

#
# ndctl_nfit_test_get_device -- create a namespace and get name of the pmem device
#                               of the nfit_test module
#
# Input argument:
# 1) mode of the namespace (devdax or fsdax)
#
function ndctl_nfit_test_get_device() {
	MODE=$1
	DEVTYPE=""
	[ "$MODE" == "devdax" ] && DEVTYPE="chardev"
	[ "$MODE" == "fsdax"  ] && DEVTYPE="blockdev"
	[ "$DEVTYPE" == "" ] && echo "ERROR: wrong namespace mode: $MODE" >&2 && exit 1

	BUS="nfit_test.0"
	REGION=$(ndctl list -b $BUS -t pmem -Ri | sed "/dev/!d;s/[\", ]//g;s/dev://g" | tail -1)
	DEVICE=$(sudo ndctl create-namespace -b $BUS -r $REGION -f -m $MODE -a 4096 | sed "/$DEVTYPE/!d;s/[\", ]//g;s/$DEVTYPE://g")
	echo $DEVICE
}

#
# ndctl_nfit_test_get_device_node -- create a namespace and get name of the pmem device
#                                    of the nfit_test module on a remote node
#
# Input argument:
# 1) mode of the namespace (devdax or fsdax)
#
function ndctl_nfit_test_get_device_node() {
	MODE=$2
	DEVTYPE=""
	[ "$MODE" == "devdax" ] && DEVTYPE="chardev"
	[ "$MODE" == "fsdax"  ] && DEVTYPE="blockdev"
	[ "$DEVTYPE" == "" ] && echo "ERROR: wrong namespace mode: $MODE" >&2 && exit 1

	BUS="nfit_test.0"
	REGION=$(expect_normal_exit run_on_node $1 ndctl list -b $BUS -t pmem -Ri | sed "/dev/!d;s/[\", ]//g;s/dev://g" | tail -1)
	DEVICE=$(expect_normal_exit run_on_node $1 sudo ndctl create-namespace -b $BUS -r $REGION -f -m $MODE -a 4096 | sed "/$DEVTYPE/!d;s/[\", ]//g;s/$DEVTYPE://g")
	echo $DEVICE
}

#
# ndctl_nfit_test_get_dax_device -- create a namespace and get name of the dax device
#                                   of the nfit_test module
#
function ndctl_nfit_test_get_dax_device() {

	# XXX needed by libndctl (it should be removed when it is not needed)
	sudo chmod o+rw /dev/ndctl*

	DEVICE=$(ndctl_nfit_test_get_device devdax)
	sudo chmod o+rw /dev/$DEVICE
	echo $DEVICE
}

#
# ndctl_nfit_test_get_dax_device_node -- create a namespace and get name of
#                                        the pmem dax device of the nfit_test
#                                        module on a remote node
#
function ndctl_nfit_test_get_dax_device_node() {
	DEVICE=$(ndctl_nfit_test_get_device_node $1 devdax)
	echo $DEVICE
}

#
# ndctl_nfit_test_get_block_device -- create a namespace and get name of the pmem block device
#                                     of the nfit_test module
#
function ndctl_nfit_test_get_block_device() {
	DEVICE=$(ndctl_nfit_test_get_device fsdax)
	echo $DEVICE
}

#
# ndctl_nfit_test_get_block_device_node -- create a namespace and get name of
#                                          the pmem block device of the nfit_test
#                                          module on a remote node
#
function ndctl_nfit_test_get_block_device_node() {
	DEVICE=$(ndctl_nfit_test_get_device_node $1 fsdax)
	echo $DEVICE
}

#
# ndctl_nfit_test_grant_access -- grant accesses required by libndctl
#
# XXX needed by libndctl (it should be removed when these extra access rights are not needed)
#
# Input argument:
# 1) a name of pmem device
#
function ndctl_nfit_test_grant_access() {
	BUS="nfit_test.0"
	REGION=$(ndctl list -b $BUS -t pmem -Ri | sed "/dev/!d;s/[\", ]//g;s/dev://g" | tail -1)
	expect_normal_exit "\
		sudo chmod o+rw /dev/nmem* && \
		sudo chmod o+r /sys/bus/nd/devices/ndbus*/$REGION/*/resource && \
		sudo chmod o+r /sys/bus/nd/devices/ndbus*/$REGION/resource"
}


#
# ndctl_nfit_test_grant_access_node -- grant accesses required by libndctl on a node
#
# XXX needed by libndctl (it should be removed when these extra access rights are not needed)
#
# Input arguments:
# 1) node number
# 2) name of pmem device
#
function ndctl_nfit_test_grant_access_node() {
	BUS="nfit_test.0"
	REGION=$(expect_normal_exit run_on_node $1 ndctl list -b $BUS -t pmem -Ri | sed "/dev/!d;s/[\", ]//g;s/dev://g" | tail -1)
	expect_normal_exit run_on_node $1 "\
		sudo chmod o+rw /dev/nmem* && \
		sudo chmod o+r /sys/bus/nd/devices/ndbus*/$REGION/*/resource && \
		sudo chmod o+r /sys/bus/nd/devices/ndbus*/$REGION/resource"
}

#
# ndctl_requires_extra_access -- checks whether ndctl will require extra
#	file permissions for bad-block iteration
#
# Input argument:
# 1) Mode of the namespace
#
function ndctl_requires_extra_access()
{
	# Tests require additional permissions for badblock iteration if they
	# are ran on device dax or with ndctl version prior to v63.
	if [ "$1" != "fsdax" ] || ! is_ndctl_ge_63 $PMEMPOOL$EXESUFFIX ; then
		return 0
	fi
	return 1
}

#
# ndctl_nfit_test_get_namespace_of_device -- get namespace of the pmem device
#
# Input argument:
# 1) a name of pmem device
#
function ndctl_get_namespace_of_device() {
	local DEVICE=$1

	NAMESPACE=$(ndctl list | grep -e "$DEVICE" -e namespace | grep -B1 -e "$DEVICE" | head -n1 | cut -d'"' -f4)
	MODE=$(ndctl list -n "$NAMESPACE" | grep mode | cut -d'"' -f4)

	if [ "$BADBLOCK_TEST_TYPE" == "nfit_test" ] && ndctl_requires_extra_access $MODE; then
		ndctl_nfit_test_grant_access $DEVICE
	fi

	echo "$NAMESPACE"
}

#
# ndctl_nfit_test_get_namespace_of_device_node -- get namespace of the pmem device on a remote node
#
# Input arguments:
# 1) node number
# 2) name of pmem device
#
function ndctl_get_namespace_of_device_node() {
	local DEVICE=$2
	NAMESPACE=$(expect_normal_exit run_on_node $1 ndctl list | grep -e "$DEVICE" -e namespace | grep -B1 -e "$DEVICE" | head -n1 | cut -d'"' -f4)
	MODE=$(expect_normal_exit run_on_node $1 ndctl list -n "$NAMESPACE" | grep mode | cut -d'"' -f4)

	if [ "$BADBLOCK_TEST_TYPE" == "nfit_test" ] && ndctl_requires_extra_access $MODE; then
		ndctl_nfit_test_grant_access_node $1 $DEVICE
	fi

	echo $NAMESPACE
}

#
# ndctl_inject_error -- inject error (bad blocks) to the namespace
#
# Input arguments:
# 1) namespace
# 2) the first bad block
# 3) number of bad blocks
#
function ndctl_inject_error() {
	local namespace=$1
	local block=$2
	local count=$3

	echo "# sudo ndctl inject-error --block=$block --count=$count $namespace" >> $PREP_LOG_FILE
	expect_normal_exit "sudo ndctl inject-error --block=$block --count=$count $namespace"  &>> $PREP_LOG_FILE

	echo "# sudo ndctl start-scrub" >> $PREP_LOG_FILE
	expect_normal_exit "sudo ndctl start-scrub" &>> $PREP_LOG_FILE

	echo "# sudo ndctl wait-scrub" >> $PREP_LOG_FILE
	expect_normal_exit "sudo ndctl wait-scrub" &>> $PREP_LOG_FILE

	echo "(done: ndctl wait-scrub)" >> $PREP_LOG_FILE
}

#
# ndctl_inject_error_node -- inject error (bad blocks) to the namespace on
#                            a given remote node
#
# Input arguments:
# 1) node
# 2) namespace
# 3) the first bad block
# 4) number of bad blocks
#
function ndctl_inject_error_node() {
	local node=$1
	local namespace=$2
	local block=$3
	local count=$4

	echo "# sudo ndctl inject-error --block=$block --count=$count $namespace" >> $PREP_LOG_FILE
	expect_normal_exit run_on_node $node "sudo ndctl inject-error --block=$block --count=$count $namespace"  &>> $PREP_LOG_FILE

	echo "# sudo ndctl start-scrub" >> $PREP_LOG_FILE
	expect_normal_exit run_on_node $node "sudo ndctl start-scrub" &>> $PREP_LOG_FILE

	echo "# sudo ndctl wait-scrub" >> $PREP_LOG_FILE
	expect_normal_exit run_on_node $node "sudo ndctl wait-scrub" &>> $PREP_LOG_FILE

	echo "(done: ndctl wait-scrub)" >> $PREP_LOG_FILE
}

#
# ndctl_uninject_error -- clear bad block error present in the namespace
#
# Input arguments:
# 1) full device name (error clearing process requires writing to device)
# 2) namespace
# 3) the first bad block
# 4) number of bad blocks
#
function ndctl_uninject_error() {
	# explicit uninjection is not required on nfit_test since any error
	# injections made during the tests are eventually cleaned up in _fini
	# function by reloading the whole namespace
	if [ "$BADBLOCK_TEST_TYPE" == "real_pmem" ]; then
		local fulldev=$1
		local namespace=$2
		local block=$3
		local count=$4
		expect_normal_exit "sudo ndctl inject-error --uninject --block=$block --count=$count $namespace &>/dev/null"
		if [ "$DEVTYPE" == "block_device" ]; then
			expect_normal_exit "sudo dd if=/dev/zero of="$fulldev" bs=512 seek="$block" count="$count" oflag=direct &>/dev/null"
		elif [ "$DEVTYPE" == "dax_device" ]; then
			expect_normal_exit "$DAXIO$EXESUFFIX -i /dev/zero -o "$fulldev" -s "$block" -l "$count" &>/dev/null"
		fi
	fi
}

#
# ndctl_uninject_error_node -- clear bad block error present in the
#                              namespace on a given remote node
#
# Input arguments:
# 1) node
# 2) full device name (error clearing process requires writing to device)
# 3) namespace
# 4) the first bad block
# 5) number of bad blocks
#
function ndctl_uninject_error_node() {
	# explicit uninjection is not required on nfit_test since any error
	# injections made during the tests are eventually cleaned up in _fini
	# function by reloading the whole namespace
	if [ "$BADBLOCK_TEST_TYPE" == "real_pmem" ]; then
		local node=$1
		local fulldev=$2
		local namespace=$3
		local block=$4
		local count=$5
		expect_normal_exit run_on_node $node "sudo ndctl inject-error --uninject --block=$block --count=$count $namespace &>/dev/null"
		if [ "$DEVTYPE" == "block_device" ]; then
			expect_normal_exit run_on_node $node "sudo dd if=/dev/zero of="$fulldev" bs=512 seek="$block" count="$count" oflag=direct &>/dev/null"
		elif [ "$DEVTYPE" == "dax_device" ]; then
			expect_normal_exit run_on_node $node "$DAXIO$EXESUFFIX -i /dev/zero -o "$fulldev" -s "$block" -l "$count" &>/dev/null"
		fi
	fi
}

#
# print_bad_blocks -- print all bad blocks (count, offset and length)
#                     in the given namespace or "No bad blocks found"
#                     if there are no bad blocks
#
# Input arguments:
# 1) namespace
#
function print_bad_blocks {
	# XXX sudo should be removed when it is not needed
	sudo ndctl list -M -n $1 | \
		grep -e "badblock_count" -e "offset" -e "length" >> $LOG \
		|| echo "No bad blocks found" >> $LOG
}

#
# expect_bad_blocks -- verify if there are required bad blocks
#                      and fail if they are not there
#
function expect_bad_blocks {
	# XXX sudo should be removed when it is not needed
	sudo ndctl list -M | grep -e "badblock_count" -e "offset" -e "length" >> $LOG && true
	if [ $? -ne 0 ]; then
		# XXX sudo should be removed when it is not needed
		sudo ndctl list -M &>> $PREP_LOG_FILE && true
		msg "====================================================================="
		msg "Error occurred, the preparation log ($PREP_LOG_FILE) is listed below:"
		msg ""
		cat $PREP_LOG_FILE
		msg "====================================================================="
		msg ""
		fatal "Error: ndctl failed to inject or retain bad blocks"
	fi
}

#
# expect_bad_blocks -- verify if there are required bad blocks
#                      and fail if they are not there
#
function expect_bad_blocks_node {
	# XXX sudo should be removed when it is not needed
	expect_normal_exit run_on_node $1 sudo ndctl list -M | \
		grep -e "badblock_count" -e "offset" -e "length" >> $LOG \
		|| fatal "Error: ndctl failed to inject or retain bad blocks (node $1)"
}
