#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2022, Intel Corporation

#
# common.sh - Library with common functions shared across gha-runers scripts
#

set -o pipefail

scriptdir=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
# Assuming that pmdk_files are in root folder of pmdk source code.
pmdkdir=$(readlink -f $scriptdir/../../)
# pmdk dir can be alse set by the global variable PMDK_DIR.
: ${PMDK_DIR=$pmdkdir}
testdir="src/test"

#
# usage_build_pmdk -- print usage detials of build pmdk function
#
function usage_build_pmdk()
{
	[[ ! -z $2 ]] && ( echo "$2"; echo ""; )
	echo "Helper function to build pmdk source for unittests."
	echo "Usage: build_pmdk [-h|--pmdk-path=[PATH]]"
	echo "-h, --help                  Print help and exit"
	echo "    --pmdk-path=[PATH]      Path to pmdk home dir (default="$PMDK_DIR")"
}

#
# build_pmdk -- build pmdk from source
#
function build_pmdk {
	local OPTIND optchar -
	while getopts ':h-:' optchar; do
		case "$optchar" in
			-)
			case "$OPTARG" in
				pmdk-path*) PMDK_DIR="${OPTARG#*=}" ;;
				help) usage_build_pmdk $0 && return 0 ;;
				*) usage_build_pmdk $0; echo "Invalid argument '$OPTARG'" && return 1 ;;
			esac
			;;
			h) usage_build_pmdk $0 && return 0 ;;
			*) usage_build_pmdk $0; echo "Invalid argument '$OPTARG'" && return 1 ;;
		esac
	done
	echo "********** make pmdk **********"
	cd $PMDK_DIR && make clean || true
	cd $PMDK_DIR && make EXTRA_CFLAGS=-DUSE_VALGRIND -j$(nproc)
	echo "********** make pmdk tests **********"
	### make unittests, sync-remotes copies files to shared folders for remote tests.
	cd $PMDK_DIR/$testdir/ && make EXTRA_CFLAGS=-DUSE_VALGRIND -j$(nproc) sync-remotes
}

#
# system_info -- print system info
#
function system_info {
	echo "********** system-info **********"
	cat /etc/os-release | grep -oP "PRETTY_NAME=\K.*"
	uname -r
	echo "libndctl: $(pkg-config --modversion libndctl)"
	echo "libfabric: $(pkg-config --modversion libfabric)"
	echo "**********memory-info**********"
	sudo ipmctl show -dimm
	sudo ipmctl show -topology
	echo "**********list-existing-namespaces**********"
	sudo ndctl list -M -N
	echo "**********installed-packages**********"
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
	sudo zypper list-updates 2>/dev/null || true
	sudo apt-get update 2>/dev/null || true ; apt upgrade --dry-run 2>/dev/null || true
	sudo dnf check-update 2>/dev/null || true
	echo "**********list-enviroment**********"
	env
}