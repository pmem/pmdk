#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2021, Intel Corporation

#
# removeNamespace.sh - Script for removing namespaces on gha runner
#

set -e

MOUNT_POINT[0]="/mnt/pmem0"
MOUNT_POINT[1]="/mnt/pmem1"

sudo umount ${MOUNT_POINT[0]} || true
sudo umount ${MOUNT_POINT[1]} || true

namespace_names=$(ndctl list -X | jq -r '.[].dev')

for n in $namespace_names
do
	sudo ndctl clear-errors $n -v
done
sudo ndctl disable-namespace all || true
sudo ndctl destroy-namespace all || true