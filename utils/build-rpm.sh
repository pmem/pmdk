#!/usr/bin/env bash
#
# Copyright 2014-2017, Intel Corporation
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
# build-rpm.sh - Script for building rpm packages
#

set -e

SCRIPT_DIR=$(dirname $0)
source $SCRIPT_DIR/pkg-common.sh

check_tool rpmbuild
check_file $SCRIPT_DIR/pkg-config.sh
source $SCRIPT_DIR/pkg-config.sh

if [ $# -lt 7 -o $# -gt 9 ]
then
	echo "Usage: $(basename $0) VERSION_TAG"\
				"SOURCE_DIR"\
				"WORKING_DIR"\
				"OUT_DIR"\
				"EXPERIMENTAL"\
				"RUN_CHECK"\
				"BUILD_RPMEM"\
				"[TEST_CONFIG_FILE]"\
				"[DISTRO] "
	exit 1
fi

PACKAGE_VERSION_TAG=$1
SOURCE=$2
WORKING_DIR=$3
OUT_DIR=$4
EXPERIMENTAL=$5
BUILD_PACKAGE_CHECK=$6
BUILD_RPMEM=$7
TEST_CONFIG_FILE=$8
DISTRO=$9


# detected distro or defined in cmd
if [ -z "${DISTRO}" ]
then
	OS=$(get_os)
	if [ "$OS" != "1" ]
	then
		echo "Detected OS: $OS"
		DISTRO=$OS
	else
		error "Unknown distribution"
		exit 1
	fi
fi


if [ "$EXTRA_CFLAGS_RELEASE" = "" ]; then
	export EXTRA_CFLAGS_RELEASE="-ggdb -fno-omit-frame-pointer"
fi

LIBFABRIC_MIN_VERSION=1.4.2
RPMBUILD_OPTS=""
PACKAGE_VERSION=$(get_version $PACKAGE_VERSION_TAG)

if [ -z "$PACKAGE_VERSION" ]
then
	error "Can not parse version from '${PACKAGE_VERSION_TAG}'"
	exit 1
fi


PACKAGE_SOURCE=${PACKAGE_NAME}-${PACKAGE_VERSION}
SOURCE=$PACKAGE_NAME
PACKAGE_TARBALL=$PACKAGE_SOURCE.tar.gz
RPM_SPEC_FILE=$PACKAGE_SOURCE/$PACKAGE_NAME.spec
MAGIC_INSTALL=$PACKAGE_SOURCE/utils/magic-install.sh
MAGIC_UNINSTALL=$PACKAGE_SOURCE/utils/magic-uninstall.sh
OLDPWD=$PWD

[ -d $WORKING_DIR ] || mkdir -v $WORKING_DIR
[ -d $OUT_DIR ] || mkdir $OUT_DIR

cd $WORKING_DIR

check_dir $SOURCE
mv $SOURCE $PACKAGE_SOURCE

if [ "$DISTRO" = "SLES" ]
then
	RPM_LICENSE="BSD-3-Clause"
	RPM_GROUP_SYS_BASE="System\/Base"
	RPM_GROUP_SYS_LIBS="System\/Libraries"
	RPM_GROUP_DEV_LIBS="Development\/Libraries\/C and C++"
	RPM_PKG_NAME_SUFFIX="1"
	RPM_MAKE_FLAGS="BINDIR=""%_bindir"" NORPATH=1"
	RPM_MAKE_INSTALL="%fdupes %{buildroot}\/%{_prefix}"
else
	RPM_LICENSE="BSD"
	RPM_GROUP_SYS_BASE="System Environment\/Base"
	RPM_GROUP_SYS_LIBS="System Environment\/Libraries"
	RPM_GROUP_DEV_LIBS="Development\/Libraries"
	RPM_PKG_NAME_SUFFIX=""
	RPM_MAKE_FLAGS="NORPATH=1"
	RPM_MAKE_INSTALL=""
fi

#
# Create parametrized spec file required by rpmbuild.
# Most of variables are set in pkg-config.sh file in order to
# keep descriptive values separately from this script.
#
sed -e "s/__VERSION__/$PACKAGE_VERSION/g" \
	-e "s/__LICENSE__/$RPM_LICENSE/g" \
	-e "s/__PACKAGE_MAINTAINER__/$PACKAGE_MAINTAINER/g" \
	-e "s/__PACKAGE_SUMMARY__/$PACKAGE_SUMMARY/g" \
	-e "s/__GROUP_SYS_BASE__/$RPM_GROUP_SYS_BASE/g" \
	-e "s/__GROUP_SYS_LIBS__/$RPM_GROUP_SYS_LIBS/g" \
	-e "s/__GROUP_DEV_LIBS__/$RPM_GROUP_DEV_LIBS/g" \
	-e "s/__PKG_NAME_SUFFIX__/$RPM_PKG_NAME_SUFFIX/g" \
	-e "s/__MAKE_FLAGS__/$RPM_MAKE_FLAGS/g" \
	-e "s/__MAKE_INSTALL_FDUPES__/$RPM_MAKE_INSTALL/g" \
	-e "s/__LIBFABRIC_MIN_VER__/$LIBFABRIC_MIN_VERSION/g" \
	$OLDPWD/$SCRIPT_DIR/nvml.spec.in > $RPM_SPEC_FILE

if [ "$DISTRO" = "SLES" ]
then
	sed -i '/^#.*bugzilla.redhat/d' $RPM_SPEC_FILE
fi

# experimental features
if [ "${EXPERIMENTAL}" = "y" ]
then
	# no experimental features for now
	RPMBUILD_OPTS+=""
fi

# librpmem & rpmemd
if [ "${BUILD_RPMEM}" = "y" ]
then
	RPMBUILD_OPTS+="--with rpmem "
else
	RPMBUILD_OPTS+="--without rpmem "
fi

# use specified testconfig file or default
if [[( -n "${TEST_CONFIG_FILE}") && ( -f "$TEST_CONFIG_FILE" ) ]]
then
	echo "Test config file: $TEST_CONFIG_FILE"
	TEST_CONFIG_VAL=${TEST_CONFIG_FILE}
else
	echo -e "Test config file $TEST_CONFIG_FILE does not exist.\n"\
		"Default test config will be used."
	TEST_CONFIG_VAL="default"
fi

# run make check or not
if [ "${BUILD_PACKAGE_CHECK}" = "y" ]
then
	CHECK=1
else
	CHECK=0
fi

tar zcf $PACKAGE_TARBALL $PACKAGE_SOURCE

# Create directory structure for rpmbuild
mkdir -v BUILD SPECS

echo "opts: $RPMBUILD_OPTS --define _testconfig ${TEST_CONFIG_VAL} --define _check ${CHECK}"

rpmbuild --define "_topdir `pwd`"\
	--define "_rpmdir ${OUT_DIR}"\
	--define "_srcrpmdir ${OUT_DIR}"\
	--define "_testconfig ${TEST_CONFIG_VAL}"\
	--define "_check ${CHECK}"\
	 -ta $PACKAGE_TARBALL \
	 $RPMBUILD_OPTS

echo "Building rpm packages done"

exit 0
