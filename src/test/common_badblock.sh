#!/usr/bin/env bash
#
# Copyright 2018, Intel Corporation
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

COMMAND_NDCTL_NFIT_TEST_INIT="\
	sudo ndctl disable-region all &>>$PREP_LOG_FILE && \
	sudo modprobe -r nfit_test &>>$PREP_LOG_FILE && \
	sudo modprobe nfit_test &>>$PREP_LOG_FILE && \
	sudo ndctl disable-region all &>>$PREP_LOG_FILE && \
	sudo ndctl zero-labels all &>>$PREP_LOG_FILE && \
	sudo ndctl enable-region all &>>$PREP_LOG_FILE"

COMMAND_NDCTL_NFIT_TEST_FINI="\
	sudo ndctl disable-region all &>>$PREP_LOG_FILE && \
	sudo modprobe -r nfit_test &>>$PREP_LOG_FILE"

#
# ndctl_nfit_test_init -- reset all regions and reload the nfit_test module
#
function ndctl_nfit_test_init() {
	expect_normal_exit $COMMAND_NDCTL_NFIT_TEST_INIT
}

#
# ndctl_nfit_test_init_node -- reset all regions and reload the nfit_test module on a remote node
#
function ndctl_nfit_test_init_node() {
	expect_normal_exit run_on_node $1 "$COMMAND_NDCTL_NFIT_TEST_INIT"
}

#
# ndctl_nfit_test_fini -- disable all regions, remove the nfit_test module
#                         and (optionally) umount the pmem block device
#
# Input argument:
# 1) pmem mount directory to be umounted
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
	[ $MOUNT_DIR ] && expect_normal_exit run_on_node $1 sudo umount $MOUNT_DIR &>> $PREP_LOG_FILE
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
# ndctl_nfit_test_get_block_device -- create a namespace and get name of the pmem block device
#                                     of the nfit_test module
#
function ndctl_nfit_test_get_block_device() {
	DEVICE=$(ndctl_nfit_test_get_device fsdax)
	echo $DEVICE
}

#
# ndctl_nfit_test_get_block_device_node -- create a namespace and get name of the pmem block device
#                                          of the nfit_test module on a remote node
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
	DEVICE=$1

	BUS="nfit_test.0"
	REGION=$(ndctl list -b $BUS -t pmem -Ri | sed "/dev/!d;s/[\", ]//g;s/dev://g" | tail -1)
	N=$(echo $REGION | cut -c7-)
	expect_normal_exit "\
		sudo chmod o+rw /dev/nmem$N && \
		sudo chmod o+r /sys/devices/platform/$BUS/ndbus1/$REGION/*/resource && \
		sudo chmod o+r /sys/devices/platform/$BUS/ndbus1/$REGION/resource"
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
	DEVICE=$1

	BUS="nfit_test.0"
	REGION=$(expect_normal_exit run_on_node $1 ndctl list -b $BUS -t pmem -Ri | sed "/dev/!d;s/[\", ]//g;s/dev://g" | tail -1)
	N=$(echo $REGION | cut -c7-)
	expect_normal_exit run_on_node $1 "\
		sudo chmod o+rw /dev/nmem$N && \
		sudo chmod o+r /sys/devices/platform/$BUS/ndbus1/$REGION/*/resource && \
		sudo chmod o+r /sys/devices/platform/$BUS/ndbus1/$REGION/resource"
}

#
# ndctl_nfit_test_get_namespace_of_device -- get namespace of the pmem device
#
# Input argument:
# 1) a name of pmem device
#
function ndctl_nfit_test_get_namespace_of_device() {
	DEVICE=$1
	NAMESPACE=$(ndctl list | grep -e "$DEVICE" -e namespace | grep -B1 -e "$DEVICE" | head -n1 | cut -d'"' -f4)

	# XXX needed by libndctl (it should be removed when it is not needed)
	ndctl_nfit_test_grant_access $DEVICE

	echo $NAMESPACE
}

#
# ndctl_nfit_test_get_namespace_of_device_node -- get namespace of the pmem device on a remote node
#
# Input arguments:
# 1) node number
# 2) name of pmem device
#
function ndctl_nfit_test_get_namespace_of_device_node() {
	DEVICE=$2
	NAMESPACE=$(expect_normal_exit run_on_node $1 ndctl list | grep -e "$DEVICE" -e namespace | grep -B1 -e "$DEVICE" | head -n1 | cut -d'"' -f4)

	# XXX needed by libndctl (it should be removed when it is not needed)
	ndctl_nfit_test_grant_access_node $1 $DEVICE

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
	local NAMESPACE=$1
	local BLOCK=$2
	local COUNT=$3
	sudo ndctl inject-error --block=$BLOCK --count=$COUNT $NAMESPACE &>> $PREP_LOG_FILE
}

#
# print_bad_blocks -- print all bad blocks (count, offset and length)
#                     or "No bad blocks found" if there are no bad blocks
#
function print_bad_blocks {
	# XXX sudo should be removed when it is not needed
	sudo ndctl list -M | grep -e "badblock_count" -e "offset" -e "length" >> $LOG || echo "No bad blocks found" >> $LOG
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
	expect_normal_exit run_on_node $1 sudo ndctl list -M | grep -e "badblock_count" -e "offset" -e "length" >> $LOG || fatal "Error: ndctl failed to inject or retain bad blocks"
}
