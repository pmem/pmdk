#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2017, Intel Corporation
#
# magic-uninstall.sh -- Script for uninstalling magic script
#
set -e

HDR_LOCAL=$(grep "File: pmdk" /etc/magic)
HDR_PKG=$(grep "File: pmdk" /usr/share/pmdk/pmdk.magic)

if [[ $HDR_LOCAL == $HDR_PKG ]]
then
	echo "Removing PMDK magic from /etc/magic"
	HDR_LINE=$(grep -n "File: pmdk" /etc/magic | cut -f1 -d:)
	HDR_PKG_LINE=$(grep -n "File: pmdk" /usr/share/pmdk/pmdk.magic | cut -f1 -d:)
	HDR_LINES=$(cat /usr/share/pmdk/pmdk.magic | wc -l)
	HDR_FIRST=$(($HDR_LINE - $HDR_PKG_LINE + 1))
	HDR_LAST=$(($HDR_FIRST + $HDR_LINES))
	sed -i "${HDR_FIRST},${HDR_LAST}d" /etc/magic
fi
