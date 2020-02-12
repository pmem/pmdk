#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2017, Intel Corporation
#
# magic-install.sh -- Script for installing magic script
#
set -e

if ! grep -q "File: pmdk" /etc/magic
then
	echo "Appending PMDK magic to /etc/magic"
	cat /usr/share/pmdk/pmdk.magic >> /etc/magic
else
	echo "PMDK magic already exists"
fi
