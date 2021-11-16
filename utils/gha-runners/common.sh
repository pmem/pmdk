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

set -o pipefail

scriptdir=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
# Assuming that pmdk_files are in root folder of pmdk source code.
pmdkdir=$(readlink -f $scriptdir/../../)
# pmdk dir can be alse set by the global variable PMDK_DIR.
: ${PMDK_DIR=$pmdkdir}
testdir="src/test"

TEST_BUILD="all"
TEST_TYPE="all"
FS_TYPE="all"
TEST_FOLDERS="all"

function filter_remotes {
	# Search for tests in folders
	local check_folder=$1
	if [ "$TEST_FOLDERS" = "remotes" ]; then
		return $(ls $check_folder | grep -q \^TEST0\$ && echo $check_folder | grep -q 'rpmem\|remote')
	elif [ "$TEST_FOLDERS" = "nonremotes" ]; then
		return $(ls $check_folder | grep -q \^TEST0\$ && ! echo $check_folder | grep -q 'rpmem\|remote')
	elif [ "$TEST_FOLDERS" = "all" ]; then
		return $(ls $check_folder | grep -q \^TEST0\$)
	fi
}

function usage_run_unittests()
{
	[[ ! -z $2 ]] && ( echo "$2"; echo ""; )
	echo "Helper function to run pmdk unittest."
	echo "Usage: run_unittests [-h|d|m|p|e|--test-build|test-type|drd|memcheck|pmemcheck|helgrind|fs-type|test-folders|test-libs=[option]|pmdk-path=[PATH]]"
	echo "-h, --help                  Print help and exit"
	echo "-d, --drd=[OPTION]          Enable valgrind DRD tests [auto,force-enable]"
	echo "-m, --memcheck=[OPTION]     Enable valgrind memcheck tests [auto,force-enable]"
	echo "-p, --pmemcheck=[OPTION]    Enable valgrind pmemcheck tests [auto,force-enable]"
	echo "-e, --helgrind=[OPTION]     Enable valgrind helgrind tests [auto,force-enable]"
	echo "    --helgrind=[OPTION]     Enable valgrind helgrind tests [auto,force-enable]"
	echo "    --pmdk-path=[PATH]      Path to pmdk home dir (default="$PMDK_DIR")"
	echo "    --test-type=[OPTION]    Run only specified test type, multiple choices can be made after colon eg. --test-type=medium,long [check(default),short,medium,long,all]"
	echo "    --test-build=[OPTION]   Run only specified build type, multiple choices can be made after colon eg. --test-build=debug,nondebug [debug,nondebug,static-debug,static-nondebug,all(default)]"
	echo "    --test-folders=[OPTION] Run remote or non-eremote tests [remotes,noneremotes,all(default)]"
	echo "    --test-libs=[OPTION]     Run only specified libs/folders"
}

function usage_build_pmdk()
{
	[[ ! -z $2 ]] && ( echo "$2"; echo ""; )
	echo "Helper function to build pmdk source for unittests."
	echo "Usage: build_pmdk [-h|--pmdk-path=[PATH]]"
	echo "-h, --help                  Print help and exit"
	echo "    --pmdk-path=[PATH]      Path to pmdk home dir (default="$PMDK_DIR")"
}

function usage_download_pmdk_archive()
{
	[[ ! -z $2 ]] && ( echo "$2"; echo ""; )
	echo "Helper function to download and extract pmdk repo from tar files."
	echo "Usage: _download_pmdk_archiv [-h|--tar-url=[url]|--tar-folder=[name]]"
	echo "-h, --help                  Print help and exit"
	echo "    --tar-url=[URL]         Url to tar file"
	echo "    --tar-folder=[NAME]     Parent folder name in tar file where pmdk repo exists, leave empty if pmdk is already top folder"
	echo "    --pmdk-path=[PATH]      Path to pmdk home dir (default="$PMDK_DIR")"
}

function run_unittests {
	local OPTIND optchar opt_args
	while getopts ':hdmpe-:' optchar; do
		case "$optchar" in
			-)
			case "$OPTARG" in
				test-build=*) TEST_BUILD="${OPTARG#*=}"; for test_build in ${TEST_BUILD//,/ }; do opt_args+=" -b $test_build"; done ;;
				test-type=*) TEST_TYPE="${OPTARG#*=}"; for test_type in ${TEST_TYPE//,/ }; do opt_args+=" -t $test_type"; done ;;
				drd=*) if [ "${OPTARG#*=}" = "force-enable" ]; then  opt_args+=" -d force-enable"; fi ;;
				memcheck=*) if [ "${OPTARG#*=}" = "force-enable" ]; then  opt_args+=" -m force-enable"; fi ;;
				pmemcheck=*) if [ "${OPTARG#*=}" = "force-enable" ]; then  opt_args+=" -p force-enable"; fi ;;
				helgrind=*) if [ "${OPTARG#*=}" = "force-enable" ]; then  opt_args+=" -e force-enable"; fi ;;
				fs-type=*) FS_TYPE="${OPTARG#*=}"; for fs_type in ${FS_TYPE//,/ }; do if [ "$fs_type" != "all" ]; then  opt_args+=" -f $fs_type"; fi ; done ;;
				test-folders=*) TEST_FOLDERS="${OPTARG#*=}" ;;
				test-libs=*) TEST_LIBS="${OPTARG#*=}" ;;
				pmdk-path*) PMDK_DIR="${OPTARG#*=}" ;;
				help) usage_run_unittests $0 && return 0 ;;
				*) usage_run_unittests $0; echo "Invalid argument '$OPTARG'" && return 1 ;;
			esac
			;;
			h) usage_run_unittests $0 && return 0 ;;
			d) opt_args+=" -d force-enable";;
			m) opt_args+=" -m force-enable";;
			p) opt_args+=" -p force-enable";;
			e) opt_args+=" -e force-enable";;
			*) usage_run_unittests $0; echo "Invalid argument '$OPTARG'" && return 1 ;;
		esac
	done
	cmd="$PMDK_DIR/$testdir/RUNTESTS $opt_args"
	cd $PMDK_DIR/$testdir/
	echo "********** Run Unittests **********"
	echo "test-cmd: KEEP_GOING=y TM=1; $cmd -o 60m [test]"

	#Test specified libs if given
	if [ "$TEST_LIBS" != '' ]; then
		for current_lib in $TEST_LIBS;
		do
			echo "********** $current_lib **********"
			KEEP_GOING=y TM=1; $cmd -o 60m $current_lib
		done
	else
		# list all folders in test dir.
		folders=$(ls -F | grep \/\$)
		for current_folder in $folders;
		do
			if filter_remotes $current_folder; then
				echo "********** $current_folder **********"
				KEEP_GOING=y TM=1; $cmd -o 60m $current_folder
			fi;
		done
	fi
}

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

function set_warning_message {
	echo $scriptdir
	cd $scriptdir &&  sudo bash -c 'cat banner >> /etc/motd'
}

function disable_warning_message {
	sudo rm /etc/motd || true
}

function update_website_content {
	echo "********** update_website_content **********"
	echo "Hostname:" `hostname`
	echo "Username:" `whoami`

	# TODO: remove hardcoded paths, add time to row filename and remove file after insert to index
	sed -i '/<!-- insert new record below -->/ r /home/tiller/charts/pmdk-val/static/row.html' /home/tiller/charts/pmdk-val/static/index.html
	cd /home/tiller/charts/pmdk-val && helm upgrade -i -f values.yaml pmdk-val . --namespace pmdk
}

 # Run update_website function via ssh on remote machine
 # @param $1 private key file
 # @param $2 user
 # @param $3 host
function update_website_content_via_ssh () {
	echo "********** update_website_content_via_ssh **********"
	typeset -f update_website_content | ssh -i $1 $2@$3 "$(cat); update_website_content"
}

function run_parallel {
	cmd_array=("$@")
	array_len=${#cmd_array[@]}
	for (( i = 0; i < $array_len; i++ ))
	do
		if [ "${cmd_array[$i]}" != '' ]; then
			#Use sed to distinguish outputs from each commands.
			echo "pmdk_thread_${i}: ${cmd_array[$i]}"
			eval "${cmd_array[$i]} | sed 's/^/[pmdk_thread_$i] /'" &
			pid_cmd[$i]=$!
		fi
	done

	for (( i = 0; i < $array_len; i++ ))
	do
		if [ "${cmd_array[$i]}" != '' ]; then
			wait ${pid_cmd[$i]}
		fi
	done
}

function download_pmdk_archive {
	local OPTIND optchar -
	while getopts ':h-:' optchar; do
		case "$optchar" in
			-)
			case "$OPTARG" in
				tar-url*) tar_url="${OPTARG#*=}" ;;
				tar-folder*) tar_folder="${OPTARG#*=}" ;;
				pmdk-path*) PMDK_DIR="${OPTARG#*=}" ;;
				help) usage_download_pmdk_archive $0 && return 0 ;;
				*) usage_download_pmdk_archive $0; echo "Invalid argument '$OPTARG'" && return 1 ;;
			esac
			;;
			h) usage_download_pmdk_archive $0 && return 0 ;;
			*) usage_download_pmdk_archive $0; echo "Invalid argument '$OPTARG'" && return 1 ;;
		esac
	done
	
	if [ -z "${tar_url}" ]; then
		echo "error: no tar url specified!"
		return 1
	fi

	mkdir --parents ${PMDK_DIR}
	wget -qO- ${tar_url} | tar zxvf - -C ${PMDK_DIR}
	if [ ! -z "${tar_folder}" ]; then
		mv ${PMDK_DIR}/${tar_folder}/* ${PMDK_DIR}
	fi
}

# Check host linux distribution and return distro name 
function check_distro {
	distro=$(cat /etc/os-release | grep -e ^NAME= | cut -c6-) && echo "${distro//\"}"
}
