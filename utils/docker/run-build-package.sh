#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2023, Intel Corporation

#
# run-build-package.sh - is called inside a Docker container; prepares
#                        the environment and starts a build of PMDK project.
#

set -e

# Prepare build environment
./prepare-for-build.sh

# Create fake tag, so that package has proper 'version' field
git config user.email "test@package.com"
git config user.name "test package"
git tag -a 1.4.99 -m "1.4" HEAD~1 || true

echo "## Build package (and run basic tests)"
pushd $WORKDIR
export PCHECK_OPTS="-j2 BLACKLIST_FILE=${BLACKLIST_FILE}"
make -j$(nproc) $PACKAGE_MANAGER

echo "## Build PMDK once more (clobber from packaging process cleared out some required files)"
make -j$(nproc)

echo "## Test built packages"
[ "$NDCTL_ENABLE" == "n" ] && extra_params="--skip-daxio" || extra_params=""
python3 $SCRIPTSDIR/test_package/test-built-packages.py -r $(pwd) ${extra_params}

echo "## Install packages"
if [[ "$PACKAGE_MANAGER" == "dpkg" ]]; then
	pushd $PACKAGE_MANAGER
	echo $USERPASS | sudo -S dpkg --install *.deb
else
	RPM_ARCH=$(uname -m)
	pushd $PACKAGE_MANAGER/$RPM_ARCH
	echo $USERPASS | sudo -S rpm --install *.rpm
fi
popd

echo "## Test installed packages"
python3 $SCRIPTSDIR/test_package/test-packages-installation.py -r $(pwd)

echo "## Compile and run standalone test"
pushd $SCRIPTSDIR/test_package
make -j$(nproc) LIBPMEMOBJ_MIN_VERSION=1.4
./test_package testfile1

echo "## Use pmreorder installed in the system"
pmreorder_version="$(pmreorder -v)"
pmreorder_pattern="pmreorder\.py .+$"
(echo "$pmreorder_version" | grep -Ev "$pmreorder_pattern") && echo "pmreorder version failed" && exit 1

touch testfile2
touch logfile1
pmreorder -p testfile2 -l logfile1
popd

echo "## Run tests (against PMDK installed in the system)"
pushd $WORKDIR/src/test
make -j$(nproc) clobber
make -j$(nproc)

# Prepare test config once more. Now, with path to PMDK set in the OS
# (rather than in the git tree) - for testing packages installed in the system.
$SCRIPTSDIR/configure-tests.sh

# Append variables exclusively for PKG tests
case "$OS" in
"opensuse/leap" | "rockylinux/rockylinux")
	PMDK_LIB_PATH_NONDEBUG=/usr/lib64
	;;
"ubuntu")
	PMDK_LIB_PATH_NONDEBUG=/lib/x86_64-linux-gnu
	;;
esac

cat << EOF >> $WORKDIR/src/test/testconfig.sh
PMDK_LIB_PATH_NONDEBUG=$PMDK_LIB_PATH_NONDEBUG
PMDK_LIB_PATH_DEBUG=$PMDK_LIB_PATH_NONDEBUG/pmdk_debug
EOF

./RUNTESTS.sh -t check
popd

popd
