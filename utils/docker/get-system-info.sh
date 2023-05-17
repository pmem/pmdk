#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation

#
# get-system-info.sh - Script for printing system info
#

function system_info {
	echo "********** system_info **********"
	cat /etc/os-release | grep -oP "PRETTY_NAME=\K.*"
	uname -r
	echo "libndctl: $(pkg-config --modversion libndctl || echo 'libndctl not found')"
	echo "valgrind: $(pkg-config --modversion valgrind || echo 'valgrind not found')"
	echo "*************** installed-packages ***************"
	# Instructions below will return some minor errors, as they are dependent on the Linux distribution.
	zypper se --installed-only 2>/dev/null || true
	apt list --installed 2>/dev/null || true
	yum list installed 2>/dev/null || true
	echo "**********/proc/cmdline**********"
	cat /proc/cmdline
	echo "**********/proc/modules**********"
	cat /proc/modules
	echo "**********/proc/cpuinfo**********"
	cat /proc/cpuinfo
	echo "**********/proc/meminfo**********"
	cat /proc/meminfo
	echo "**********/proc/swaps**********"
	cat /proc/swaps
	echo "**********/proc/version**********"
	cat /proc/version
	echo "**********check-updates**********"
	# Instructions below will return some minor errors, as they are dependent on the Linux distribution.
	zypper list-updates 2>/dev/null || true
	apt-get update 2>/dev/null || true
	apt upgrade --dry-run 2>/dev/null || true
	dnf check-update 2>/dev/null || true
	echo "**********list-enviroment**********"
	env
	echo "******list-build-system-versions*******"
	gcc --version 2>/dev/null || true
	clang --version 2>/dev/null || true
	make --version2>/dev/null || true
}

# Call the function above to print system info.
system_info
