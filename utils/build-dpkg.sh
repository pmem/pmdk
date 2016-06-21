#!/bin/bash -e
#
# Copyright 2014-2016, Intel Corporation
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
# build-dpkg.sh - Script for building deb packages
#

SCRIPT_DIR=$(dirname $0)
source $SCRIPT_DIR/pkg-common.sh

if [ $# -lt 6 -o $# -gt 7 ]
then
	echo "Usage: $(basename $0) VERSION_TAG SOURCE_DIR WORKING_DIR"\
					"OUT_DIR EXPERIMENTAL RUN_CHECK"\
					"[TEST_CONFIG_FILE] "
	exit 1
fi

PACKAGE_VERSION_TAG=$1
SOURCE=$2
WORKING_DIR=$3
OUT_DIR=$4
EXPERIMENTAL=$5
BUILD_PACKAGE_CHECK=$6
TEST_CONFIG_FILE=$7

function convert_changelog() {
	while read line
	do
		if [[ $line =~ $REGEX_DATE_AUTHOR ]]
		then
			DATE="${BASH_REMATCH[1]}"
			AUTHOR="${BASH_REMATCH[2]}"
			echo "  * ${DATE} ${AUTHOR}"
		elif [[ $line =~ $REGEX_MESSAGE_START ]]
		then
			MESSAGE="${BASH_REMATCH[1]}"
			echo "  - ${MESSAGE}"
		elif [[ $line =~ $REGEX_MESSAGE ]]
		then
			MESSAGE="${BASH_REMATCH[1]}"
			echo "    ${MESSAGE}"
		fi
	done < $1
}

function experimental_install_triggers_overrides() {
cat << EOF > debian/${OBJ_CPP_NAME}.install
usr/include/libpmemobj/*.hpp
usr/include/libpmemobj/detail/*.hpp
usr/share/doc/${OBJ_CPP_DOC_DIR}/*
EOF

cat << EOF > debian/${OBJ_CPP_NAME}.triggers
interest doc-base
EOF

cat << EOF > debian/${OBJ_CPP_NAME}.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
EOF

cat << EOF > debian/${OBJ_CPP_NAME}.doc-base
Document: ${OBJ_CPP_NAME}
Title: NVML libpmemobj C++ bindings Manual
Author: NVML Developers
Abstract: This is the HTML docs for the C++ bindings for NVML's libpmemobj.
Section: Programming

Format: HTML
Index: /usr/share/doc/${OBJ_CPP_DOC_DIR}/index.html
Files: /usr/share/doc/${OBJ_CPP_DOC_DIR}/*
EOF
}

function append_experimental_control() {
cat << EOF >> $CONTROL_FILE

Package: ${OBJ_CPP_NAME}
Section: libdevel
Architecture: any
Depends: libpmemobj-dev (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: C++ bindings for libpmemobj (EXPERIMENTAL)
 Headers-only C++ library for libpmemobj.
EOF
}

CHECK_CMD="
override_dh_auto_test:
	dh_auto_test
	if [ -f $TEST_CONFIG_FILE ]; then\
		cp $TEST_CONFIG_FILE src/test/testconfig.sh;\
	else\
	        cp src/test/testconfig.sh.example src/test/testconfig.sh;\
	fi
	make check
"

if [ "${BUILD_PACKAGE_CHECK}" != "y" ]
then
	CHECK_CMD=""
fi

check_tool debuild
check_tool dch
check_file $SCRIPT_DIR/pkg-config.sh

source $SCRIPT_DIR/pkg-config.sh

PACKAGE_VERSION=$(get_version $PACKAGE_VERSION_TAG)
PACKAGE_RELEASE=1
PACKAGE_SOURCE=${PACKAGE_NAME}-${PACKAGE_VERSION}
PACKAGE_TARBALL_ORIG=${PACKAGE_NAME}_${PACKAGE_VERSION}.orig.tar.gz
MAGIC_INSTALL=utils/magic-install.sh
MAGIC_UNINSTALL=utils/magic-uninstall.sh
CONTROL_FILE=debian/control
OBJ_CPP_NAME=libpmemobj++-dev
OBJ_CPP_DOC_DIR=${OBJ_CPP_NAME}-${PACKAGE_VERSION}

[ -d $WORKING_DIR ] || mkdir $WORKING_DIR
[ -d $OUT_DIR ] || mkdir $OUT_DIR

OLD_DIR=$PWD

cd $WORKING_DIR

check_dir $SOURCE

mv $SOURCE $PACKAGE_SOURCE
tar zcf $PACKAGE_TARBALL_ORIG $PACKAGE_SOURCE

cd $PACKAGE_SOURCE

mkdir debian

# Generate compat file
cat << EOF > debian/compat
9
EOF

# Generate control file
cat << EOF > $CONTROL_FILE
Source: $PACKAGE_NAME
Maintainer: $PACKAGE_MAINTAINER
Section: misc
Priority: optional
Standards-version: 3.9.4
Build-Depends: debhelper (>= 9)

Package: libpmem
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: NVML libpmem library
 NVML library for Persistent Memory support

Package: libpmem-dev
Section: libdevel
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmem
 Development files for libpmem library.

Package: libpmemblk
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: NVML libpmemblk library
 NVML library for Persistent Memory support - block memory pool.

Package: libpmemblk-dev
Section: libdevel
Architecture: any
Depends: libpmemblk (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmemblk
 Development files for libpmemblk library.

Package: libpmemlog
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: NVML libpmemlog library
 NVML library for Persistent Memory support - log memory pool.

Package: libpmemlog-dev
Section: libdevel
Architecture: any
Depends: libpmemlog (=\${binary:Version}),  \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmemlog
 Development files for libpmemlog library.

Package: libpmemobj
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: NVML libpmemobj library
 NVML library for Persistent Memory support - transactional object store.

Package: libpmemobj-dev
Section: libdevel
Architecture: any
Depends: libpmemobj (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmemobj
 Development files for libpmemobj-dev library.

Package: libpmempool
Architecture: any
Depends: libpmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: NVML libpmempool library
 NVML library for Persistent Memory support - pool management library.

Package: libpmempool-dev
Section: libdevel
Architecture: any
Depends: libpmempool (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libpmempool
 Development files for libpmempool library.

Package: libvmem
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: NVML libvmem library
 NVML library for volatile-memory support on top of Persistent Memory.

Package: libvmem-dev
Section: libdevel
Architecture: any
Depends: libvmem (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libvmem
 Development files for libvmem library.

Package: libvmmalloc
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: NVML libvmmalloc library
 NVML general purpose volatile-memory allocation library on top of Persistent Memory.

Package: libvmmalloc-dev
Section: libdevel
Architecture: any
Depends: libvmmalloc (=\${binary:Version}), \${shlibs:Depends}, \${misc:Depends}
Description: Development files for libvmmalloc
 Development files for libvmmalloc library.

Package: $PACKAGE_NAME-dbg
Section: debug
Priority: extra
Architecture: any
Depends: libvmem (=\${binary:Version}), libvmmalloc (=\${binary:Version}), libpmem (=\${binary:Version}), libpmemblk (=\${binary:Version}), libpmemlog (=\${binary:Version}), libpmemobj (=\${binary:Version}), libpmempool (=\${binary:Version}), \${misc:Depends}
Description: Debug symbols for NVML libraries
 Debug symbols for all NVML libraries.

Package: $PACKAGE_NAME-tools
Section: misc
Architecture: any
Priority: optional
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: Tools for $PACKAGE_NAME
 Utilities for $PACKAGE_NAME.
EOF

cp LICENSE debian/copyright

cat << EOF > debian/rules
#!/usr/bin/make -f
#export DH_VERBOSE=1
%:
	dh \$@

override_dh_strip:
	dh_strip --dbg-package=$PACKAGE_NAME-dbg

override_dh_auto_install:
	dh_auto_install -- EXPERIMENTAL=${EXPERIMENTAL} CPP_DOC_DIR="${OBJ_CPP_DOC_DIR}" prefix=/usr sysconfdir=/etc

override_dh_install:
	mkdir -p debian/tmp/usr/share/nvml/
	cp utils/nvml.magic debian/tmp/usr/share/nvml/
	dh_install

${CHECK_CMD}
EOF

chmod +x debian/rules

mkdir debian/source

ITP_BUG_EXCUSE="# This is our first package but we do not want to upload it yet.
# Please refer to Debian Developer's Reference section 5.1 (New packages) for details:
# https://www.debian.org/doc/manuals/developers-reference/pkgs.html#newpackage"

cat << EOF > debian/source/format
3.0 (quilt)
EOF

cat << EOF > debian/libpmem.install
usr/lib/libpmem.so.*
usr/share/nvml/nvml.magic
EOF

cat $MAGIC_INSTALL > debian/libpmem.postinst
cat $MAGIC_UNINSTALL > debian/libpmem.prerm

cat << EOF > debian/libpmem.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libpmem: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libpmem-dev.install
usr/lib/nvml_debug/libpmem.a usr/lib/nvml_dbg/
usr/lib/nvml_debug/libpmem.so	usr/lib/nvml_dbg/
usr/lib/nvml_debug/libpmem.so.* usr/lib/nvml_dbg/
usr/lib/libpmem.so
usr/lib/pkgconfig/libpmem.pc
usr/include/libpmem.h
usr/share/man/man3/libpmem.3.gz
EOF

cat << EOF > debian/libpmem-dev.triggers
interest man-db
EOF

cat << EOF > debian/libpmem-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions usr/lib/nvml_dbg/*
EOF

cat << EOF > debian/libpmemblk.install
usr/lib/libpmemblk.so.*
EOF

cat << EOF > debian/libpmemblk.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libpmemblk: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libpmemblk-dev.install
usr/lib/nvml_debug/libpmemblk.a usr/lib/nvml_dbg/
usr/lib/nvml_debug/libpmemblk.so usr/lib/nvml_dbg/
usr/lib/nvml_debug/libpmemblk.so.* usr/lib/nvml_dbg/
usr/lib/libpmemblk.so
usr/lib/pkgconfig/libpmemblk.pc
usr/include/libpmemblk.h
usr/share/man/man3/libpmemblk.3.gz
EOF

cat << EOF > debian/libpmemblk-dev.triggers
interest man-db
EOF

cat << EOF > debian/libpmemblk-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions usr/lib/nvml_dbg/*
EOF

cat << EOF > debian/libpmemlog.install
usr/lib/libpmemlog.so.*
EOF

cat << EOF > debian/libpmemlog.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libpmemlog: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libpmemlog-dev.install
usr/lib/nvml_debug/libpmemlog.a usr/lib/nvml_dbg/
usr/lib/nvml_debug/libpmemlog.so usr/lib/nvml_dbg/
usr/lib/nvml_debug/libpmemlog.so.* usr/lib/nvml_dbg/
usr/lib/libpmemlog.so
usr/lib/pkgconfig/libpmemlog.pc
usr/include/libpmemlog.h
usr/share/man/man3/libpmemlog.3.gz
EOF

cat << EOF > debian/libpmemlog-dev.triggers
interest man-db
EOF

cat << EOF > debian/libpmemlog-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions usr/lib/nvml_dbg/*
EOF

cat << EOF > debian/libpmemobj.install
usr/lib/libpmemobj.so.*
EOF

cat << EOF > debian/libpmemobj.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libpmemobj: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libpmemobj-dev.install
usr/lib/nvml_debug/libpmemobj.a usr/lib/nvml_dbg/
usr/lib/nvml_debug/libpmemobj.so usr/lib/nvml_dbg/
usr/lib/nvml_debug/libpmemobj.so.* usr/lib/nvml_dbg/
usr/lib/libpmemobj.so
usr/lib/pkgconfig/libpmemobj.pc
usr/include/libpmemobj.h
usr/share/man/man3/libpmemobj.3.gz
EOF

cat << EOF > debian/libpmemobj-dev.triggers
interest man-db
EOF

cat << EOF > debian/libpmemobj-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions usr/lib/nvml_dbg/*
EOF

cat << EOF > debian/libpmempool.install
usr/lib/libpmempool.so.*
EOF

cat << EOF > debian/libpmempool.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libpmempool: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libpmempool-dev.install
usr/lib/nvml_debug/libpmempool.a usr/lib/nvml_dbg/
usr/lib/nvml_debug/libpmempool.so usr/lib/nvml_dbg/
usr/lib/nvml_debug/libpmempool.so.* usr/lib/nvml_dbg/
usr/lib/libpmempool.so
usr/lib/pkgconfig/libpmempool.pc
usr/include/libpmempool.h
usr/share/man/man3/libpmempool.3.gz
EOF

cat << EOF > debian/libpmempool-dev.triggers
interest man-db
EOF

cat << EOF > debian/libpmempool-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions usr/lib/nvml_dbg/*
EOF

cat << EOF > debian/libvmem.install
usr/lib/libvmem.so.*
EOF

cat << EOF > debian/libvmem.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libvmem: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libvmem-dev.install
usr/lib/nvml_debug/libvmem.a usr/lib/nvml_dbg/
usr/lib/nvml_debug/libvmem.so	usr/lib/nvml_dbg/
usr/lib/nvml_debug/libvmem.so.* usr/lib/nvml_dbg/
usr/lib/libvmem.so
usr/lib/pkgconfig/libvmem.pc
usr/include/libvmem.h
usr/share/man/man3/libvmem.3.gz
EOF

cat << EOF > debian/libvmem-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions usr/lib/nvml_dbg/*
EOF

cat << EOF > debian/libvmem-dev.triggers
interest man-db
EOF

cat << EOF > debian/libvmmalloc.install
usr/lib/libvmmalloc.so.*
EOF

cat << EOF > debian/libvmmalloc.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
libvmmalloc: package-name-doesnt-match-sonames
EOF

cat << EOF > debian/libvmmalloc-dev.install
usr/lib/nvml_debug/libvmmalloc.a   usr/lib/nvml_dbg/
usr/lib/nvml_debug/libvmmalloc.so   usr/lib/nvml_dbg/
usr/lib/nvml_debug/libvmmalloc.so.* usr/lib/nvml_dbg/
usr/lib/libvmmalloc.so
usr/lib/pkgconfig/libvmmalloc.pc
usr/include/libvmmalloc.h
usr/share/man/man3/libvmmalloc.3.gz
EOF

cat << EOF > debian/libvmmalloc-dev.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
# The following warnings are triggered by a bug in debhelper:
# http://bugs.debian.org/204975
postinst-has-useless-call-to-ldconfig
postrm-has-useless-call-to-ldconfig
# We do not want to compile with -O2 for debug version
hardening-no-fortify-functions usr/lib/nvml_dbg/*
EOF

cat << EOF > debian/libvmmalloc-dev.triggers
interest man-db
EOF

cat << EOF > debian/$PACKAGE_NAME-dbg.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
EOF

cat << EOF > debian/$PACKAGE_NAME-tools.install
usr/bin/pmempool
usr/share/man/man1/pmempool.1.gz
usr/share/man/man1/pmempool-create.1.gz
usr/share/man/man1/pmempool-info.1.gz
usr/share/man/man1/pmempool-dump.1.gz
usr/share/man/man1/pmempool-check.1.gz
usr/share/man/man1/pmempool-rm.1.gz
usr/share/man/man1/pmempool-convert.1.gz
etc/bash_completion.d/pmempool.sh
EOF

cat << EOF > debian/$PACKAGE_NAME-tools.triggers
interest man-db
EOF

cat << EOF > debian/$PACKAGE_NAME-tools.lintian-overrides
$ITP_BUG_EXCUSE
new-package-should-close-itp-bug
EOF

# Experimental features
if [ "${EXPERIMENTAL}" = "y" ]
then
	append_experimental_control;
	experimental_install_triggers_overrides;
fi

# Convert ChangeLog to debian format
CHANGELOG_TMP=changelog.tmp
dch --create --empty --package $PACKAGE_NAME -v $PACKAGE_VERSION-$PACKAGE_RELEASE -M -c $CHANGELOG_TMP
touch debian/changelog
head -n1 $CHANGELOG_TMP >> debian/changelog
echo "" >> debian/changelog
convert_changelog ChangeLog >> debian/changelog
echo "" >> debian/changelog
tail -n1 $CHANGELOG_TMP >> debian/changelog
rm $CHANGELOG_TMP

# This is our first release but we do
debuild -us -uc

cd $OLD_DIR

find $WORKING_DIR -name "*.deb"\
              -or -name "*.dsc"\
	      -or -name "*.changes"\
	      -or -name "*.orig.tar.gz"\
	      -or -name "*.debian.tar.gz" | while read FILE
do
	mv -v $FILE $OUT_DIR/
done

exit 0
