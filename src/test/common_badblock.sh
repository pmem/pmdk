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

function ndctl_inject_error() {
	local NAMESPACE=$1
	local BLOCK=$2
	local COUNT=$3
	ndctl inject-error --block=$BLOCK --count=$COUNT $NAMESPACE &>/dev/null
}


function ndctl_nfit_test_enable() {
	ndctl disable-region all &>> $PREP_LOG_FILE
	modprobe -r nfit_test
	modprobe nfit_test
	ndctl disable-region all &>> $PREP_LOG_FILE
	ndctl zero-labels all &>> $PREP_LOG_FILE
	ndctl enable-region all &>> $PREP_LOG_FILE
}

function ndctl_nfit_test_disable() {
	MOUNT_DIR=$1
	[ $MOUNT_DIR ] && umount $MOUNT_DIR &>> $PREP_LOG_FILE
	ndctl disable-region all &>> $PREP_LOG_FILE
	modprobe -r nfit_test
}

function ndctl_nfit_test_get_device() {
	MODE=$1
	DEVTYPE=$2
	BUS="nfit_test.0"
	REGION=$(ndctl list -b $BUS -t pmem -Ri | sed "/dev/!d;s/[\", ]//g;s/dev://g" | tail -1)
	DEVICE=$(ndctl create-namespace -b $BUS -r $REGION -f -m $MODE -a 4096 | sed "/$DEVTYPE/!d;s/[\", ]//g;s/$DEVTYPE://g")
	echo $DEVICE
}

function ndctl_nfit_test_get_dax_device() {
	DEVICE=$(ndctl_nfit_test_get_device dax chardev)
	echo $DEVICE
}

function ndctl_nfit_test_get_block_device() {
	DEVICE=$(ndctl_nfit_test_get_device memory blockdev)
	echo $DEVICE
}

function ndctl_nfit_test_get_namespace_of_device() {
	DEVICE=$1
	NAMESPACE=$(ndctl list | grep -e "$DEVICE" -e namespace | grep -B1 -e "$DEVICE" | head -n1 | cut -d'"' -f4)
	echo $NAMESPACE
}

function ndctl_nfit_test_mount_pmem() {
	FULLDEV=$1
	MOUNT_DIR=$2

	mkfs.ext4 $FULLDEV &>> $PREP_LOG_FILE
	mkdir -p $MOUNT_DIR
	mount $FULLDEV $MOUNT_DIR
}
