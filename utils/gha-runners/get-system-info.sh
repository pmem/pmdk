#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

#
# get-system-info.sh - Script for printing system info
#

function system_info {
	echo "********** system_info **********"
	cat /etc/os-release | grep -oP "PRETTY_NAME=\K.*"
	uname -r
	echo "libndctl: $(pkg-config --modversion libndctl || echo 'libndctl not found')"
	echo "valgrind: $(pkg-config --modversion valgrind || echo 'valgrind not found')"
	echo "******************** memory-info *******************"
	sudo ipmctl show -dimm || true
	sudo ipmctl show -topology || true
	echo "*************** list-existing-namespaces ***************"
	sudo ndctl list -M -N
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
	sudo zypper list-updates 2>/dev/null || true
	sudo apt-get update 2>/dev/null || true
	sudo apt upgrade --dry-run 2>/dev/null || true
	sudo dnf check-update 2>/dev/null || true
	echo "**********list-enviroment**********"
	env
}

# Call the function above to print system info.
system_info
