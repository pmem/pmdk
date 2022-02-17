#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation
#

#
# run-build.sh - is called inside a Docker container,
#                starts libminiasync build with tests.
#

set -e

PREFIX=/usr
CC=${CC:-gcc}
CHECK_CSTYLE=${CHECK_CSTYLE:-ON}
TEST_DIR=${LIBMINIASYNC_TEST_DIR:-${DEFAULT_TEST_DIR}}
EXAMPLE_TEST_DIR="/tmp/libminiasync_example_build"

# turn off sanitizers only if (CI_SANITS == OFF)
[ "$CI_SANITS" != "OFF" ] && CI_SANITS=ON

if [ "$WORKDIR" == "" ]; then
	echo "Error: WORKDIR is not set"
	exit 1
fi

if [ "$TEST_DIR" == "" ]; then
	echo "Error: LIBMINIASYNC_TEST_DIR is not set"
	exit 1
fi

case "$PACKAGE_MANAGER" in
	"rpm"|"deb") # supported package managers
		;;
	"none") # no package manager, install the library from sources
		echo "Notice: the miniasync library will be installed from sources"
		PACKAGE_MANAGER=""
		;;
	"")
		echo "Error: PACKAGE_MANAGER is not set"
		echo "       Use 'rpm' or 'deb' to build packages or 'none' to install the library from sources."
		exit 1
		;;
	*)
		# unsupported package manager
		echo "Error: unsupported PACKAGE_MANAGER: $PACKAGE_MANAGER"
		echo "       Use 'rpm' or 'deb' to build packages or 'none' to install the library from sources."
		exit 1
		;;
esac

function sudo_password() {
	echo $USERPASS | sudo -Sk $*
}

function upload_codecov() {
	printf "\n$(tput setaf 1)$(tput setab 7)COVERAGE ${FUNCNAME[0]} START$(tput sgr 0)\n"

	# set proper gcov command
	clang_used=$(cmake -LA -N . | grep CMAKE_C_COMPILER | grep clang | wc -c)
	if [[ $clang_used > 0 ]]; then
		gcovexe="llvm-cov gcov"
	else
		gcovexe="gcov"
	fi

	# run gcov exe, using their bash (remove parsed coverage files, set flag and exit 1 if not successful)
	# we rely on parsed report on codecov.io; the output is quite long, hence it's disabled using -X flag
	/opt/scripts/codecov -c -F $1 -Z -x "$gcovexe" -X "gcovout"

	printf "check for any leftover gcov files\n"
	leftover_files=$(find . -name "*.gcov" | wc -l)
	if [[ $leftover_files > 0 ]]; then
		# display found files and exit with error (they all should be parsed)
		find . -name "*.gcov"
		return 1
	fi

	printf "$(tput setaf 1)$(tput setab 7)COVERAGE ${FUNCNAME[0]} END$(tput sgr 0)\n\n"
}

function compile_example_standalone() {
	rm -rf $EXAMPLE_TEST_DIR
	mkdir $EXAMPLE_TEST_DIR
	cd $EXAMPLE_TEST_DIR

	cmake $1

	# exit on error
	if [[ $? != 0 ]]; then
		cd - > /dev/null
		return 1
	fi

	make -j$(nproc)
	cd - > /dev/null
}

# test standalone compilation of all examples
function test_compile_all_examples_standalone() {

	EXAMPLES=$(ls -1 $WORKDIR/examples/)
	for e in $EXAMPLES; do
		DIR=$WORKDIR/examples/$e
		[ ! -d $DIR ] && continue
		[ ! -f $DIR/CMakeLists.txt ] && continue
		echo
		echo "###########################################################"
		echo "### Testing standalone compilation of example: $e"
		if [ "$PACKAGE_MANAGER" = "" ]; then
			echo "### (with miniasync installed from RELEASE sources)"
		else
			echo "### (with miniasync installed from RELEASE packages)"
		fi
		echo "###########################################################"
		compile_example_standalone $DIR
	done
}

./prepare-for-build.sh

echo
echo "##################################################################"
echo "### Verify build with ASAN and UBSAN ($CC, DEBUG)"
echo "##################################################################"

mkdir -p $WORKDIR/build
cd $WORKDIR/build

CC=$CC \
cmake .. -DCMAKE_BUILD_TYPE=Debug \
	-DCHECK_CSTYLE=${CHECK_CSTYLE} \
	-DDEVELOPER_MODE=1 \
	-DUSE_ASAN=${CI_SANITS} \
	-DUSE_UBSAN=${CI_SANITS} \
	-DTEST_DIR=$TEST_DIR
make -j$(nproc)
ctest --output-on-failure

cd $WORKDIR
rm -rf $WORKDIR/build

echo
echo "##################################################################"
echo "### Verify build with ASAN and UBSAN ($CC, DEBUG, DML)"
echo "##################################################################"

mkdir -p $WORKDIR/build
cd $WORKDIR/build

CC=$CC \
cmake .. -DCMAKE_BUILD_TYPE=Debug \
	-DCHECK_CSTYLE=${CHECK_CSTYLE} \
	-DDEVELOPER_MODE=1 \
	-DUSE_ASAN=${CI_SANITS} \
	-DUSE_UBSAN=${CI_SANITS} \
	-DTEST_DIR=$TEST_DIR \
	-DCOMPILE_DML=1
make -j$(nproc)
ctest --output-on-failure

cd $WORKDIR
rm -rf $WORKDIR/build

echo
echo "##################################################################"
echo "### Verify build and install (in dir: ${PREFIX}) ($CC, DEBUG)"
echo "##################################################################"

mkdir -p $WORKDIR/build
cd $WORKDIR/build

CC=$CC \
cmake .. -DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_INSTALL_PREFIX=$PREFIX \
	-DCOVERAGE=$COVERAGE \
	-DCHECK_CSTYLE=${CHECK_CSTYLE} \
	-DDEVELOPER_MODE=1 \
	-DTEST_DIR=$TEST_DIR
make -j$(nproc)
ctest --output-on-failure
sudo_password -S make -j$(nproc) install

if [ "$COVERAGE" == "1" ]; then
	upload_codecov tests
fi

# Create a PR with generated docs
if [ "$AUTO_DOC_UPDATE" == "1" ]; then
	echo "Running auto doc update"
	../utils/docker/run-doc-update.sh
fi

#test_compile_all_examples_standalone

# Uninstall libraries
cd $WORKDIR/build
sudo_password -S make uninstall

cd $WORKDIR
rm -rf $WORKDIR/build

echo
echo "##################################################################"
echo "### Verify build and install (in dir: ${PREFIX}) ($CC, DEBUG, DML)"
echo "##################################################################"

mkdir -p $WORKDIR/build
cd $WORKDIR/build

CC=$CC \
cmake .. -DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_INSTALL_PREFIX=$PREFIX \
	-DCOVERAGE=$COVERAGE \
	-DCHECK_CSTYLE=${CHECK_CSTYLE} \
	-DDEVELOPER_MODE=1 \
	-DTEST_DIR=$TEST_DIR \
	-DCOMPILE_DML=1
make -j$(nproc)
ctest --output-on-failure
sudo_password -S make -j$(nproc) install

if [ "$COVERAGE" == "1" ]; then
	upload_codecov tests
fi

# Create a PR with generated docs
if [ "$AUTO_DOC_UPDATE" == "1" ]; then
	echo "Running auto doc update"
	../utils/docker/run-doc-update.sh
fi

#test_compile_all_examples_standalone

# Uninstall libraries
cd $WORKDIR/build
sudo_password -S make uninstall

cd $WORKDIR
rm -rf $WORKDIR/build

echo
echo "##################################################################"
echo "### Verify build and install (in dir: ${PREFIX}) ($CC, RELEASE)"
echo "##################################################################"

mkdir -p $WORKDIR/build
cd $WORKDIR/build

CC=$CC \
cmake .. -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=$PREFIX \
	-DCPACK_GENERATOR=$PACKAGE_MANAGER \
	-DCHECK_CSTYLE=${CHECK_CSTYLE} \
	-DDEVELOPER_MODE=1 \
	-DTEST_DIR=$TEST_DIR
make -j$(nproc)
ctest --output-on-failure

if [ "$PACKAGE_MANAGER" = "" ]; then
	# install the library from sources
	sudo_password -S make -j$(nproc) install
else
	# Do not install the library from sources here,
	# because it will be installed from the packages below.

	echo "##############################################################"
	echo "### Making and testing packages (RELEASE version) ..."
	echo "##############################################################"

	make -j$(nproc) package
	find . -iname "libminiasync*.$PACKAGE_MANAGER"
fi

if [ $PACKAGE_MANAGER = "deb" ]; then
	echo "$ dpkg-deb --info ./libminiasync*.deb"
	dpkg-deb --info ./libminiasync*.deb

	echo "$ dpkg-deb -c ./libminiasync*.deb"
	dpkg-deb -c ./libminiasync*.deb

	echo "$ sudo -S dpkg -i ./libminiasync*.deb"
	echo $USERPASS | sudo -S dpkg -i ./libminiasync*.deb

elif [ $PACKAGE_MANAGER = "rpm" ]; then
	echo "$ rpm -q --info ./libminiasync*.rpm"
	rpm -q --info ./libminiasync*.rpm && true

	echo "$ rpm -q --list ./libminiasync*.rpm"
	rpm -q --list ./libminiasync*.rpm && true

	echo "$ sudo -S rpm -ivh --force *.rpm"
	echo $USERPASS | sudo -S rpm -ivh --force *.rpm
fi

#test_compile_all_examples_standalone

if [ "$PACKAGE_MANAGER" = "" ]; then
	:
	# uninstall the library, since it was installed from sources
	cd $WORKDIR/build
	sudo_password -S make uninstall
elif [ $PACKAGE_MANAGER = "deb" ]; then
	echo "sudo -S dpkg --remove libminiasync-dev"
	echo $USERPASS | sudo -S dpkg --remove libminiasync-dev
elif [ $PACKAGE_MANAGER = "rpm" ]; then
	echo "$ sudo -S rpm --erase libminiasync-devel"
	echo $USERPASS | sudo -S rpm --erase libminiasync-devel
fi

cd $WORKDIR
rm -rf $WORKDIR/build
