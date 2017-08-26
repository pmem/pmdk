#!/bin/bash -e
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

SCRIPT_DIR=$(dirname $0)
source $SCRIPT_DIR/pkg-common.sh

if [ $# -lt 6 -o $# -gt 8 ]
then
        echo "Usage: $(basename $0) VERSION_TAG SOURCE_DIR WORKING_DIR"\
                                        "OUT_DIR EXPERIMENTAL RUN_CHECK"\
                                        "[TEST_CONFIG_FILE] [DAXEMU_CFG_FILE]"
        exit 1
fi

PACKAGE_VERSION_TAG=$1
SOURCE=$2
WORKING_DIR=$3
OUT_DIR=$4
EXPERIMENTAL=$5
BUILD_PACKAGE_CHECK=$6
TEST_CONFIG_FILE=$7
DAXEMU_CFG_FILE=$8
if [ "$EXTRA_CFLAGS_RELEASE" = "" ]; then
	export EXTRA_CFLAGS_RELEASE="-ggdb -fno-omit-frame-pointer"
fi
LIBFABRIC_MIN_VERSION=1.4.2

function create_changelog() {
	echo
	echo "%changelog"
	while read
	do
		if [[ $REPLY =~ $REGEX_DATE_AUTHOR ]]
		then
			DATE=$(date --date="${BASH_REMATCH[1]}" '+%a %b %d %Y')
			AUTHOR="${BASH_REMATCH[2]}"
			echo "* ${DATE} ${AUTHOR}"
		elif [[ $REPLY =~ $REGEX_MESSAGE_START ]]
		then
			echo "- ${BASH_REMATCH[1]}"
		elif [[ $REPLY =~ $REGEX_MESSAGE ]]
		then
			echo "  ${BASH_REMATCH[1]}"
		fi
	done < $1
}

function add_rpmem_packages() {
cat << EOF >> $RPM_SPEC_FILE
%package -n librpmem
Summary: librpmem library
Group: %{package_group}/Libraries
Requires: libfabric >= $LIBFABRIC_MIN_VERSION
%description -n librpmem
NVML librpmem library

%files -n librpmem
%defattr(-,root,root,-)
%{_libdir}/librpmem.so.*

%package -n librpmem-devel
Summary: librpmem development library
Group: Development/Libraries
Requires: librpmem >= $PACKAGE_VERSION
%description -n librpmem-devel
Development files for NVML librpmem library

%files -n librpmem-devel
%defattr(-,root,root,-)
%{_libdir}/librpmem.so
%{_libdir}/librpmem.a
%{_libdir}/pkgconfig/librpmem.pc
%{_libdir}/nvml_debug/librpmem.so
%{_libdir}/nvml_debug/librpmem.so.*
%{_libdir}/nvml_debug/librpmem.a
%{_includedir}/librpmem.h
%{_mandir}/man3/librpmem.3.gz

%package -n rpmemd
Group:		%{package_group}
Summary:	Remote Access to Persistent Memory daemon
Requires:	libfabric >= $LIBFABRIC_MIN_VERSION

%description -n rpmemd
Remote Access to Persistent Memory daemon

%files -n rpmemd
%{_bindir}/rpmemd
%{_mandir}/man1/rpmemd.1.gz

EOF
}

check_tool rpmbuild
check_file $SCRIPT_DIR/pkg-config.sh

source $SCRIPT_DIR/pkg-config.sh

PACKAGE_VERSION=$(get_version $PACKAGE_VERSION_TAG)
# Release tag can be thought of as the package's version - not source.
# Therefore we set this value to 1 as we always build package from well
# defined version of the source. This should be incremented when rebuilding
# package with any patches specified for target platform/distro.
PACKAGE_RELEASE=1

if [ -z "$PACKAGE_VERSION" ]
then
	error "Can not parse version from '${PACKAGE_VERSION_TAG}'"
	exit 1
fi

CHECK_CMD="
%check
if [ -f $TEST_CONFIG_FILE ]; then
	cp $TEST_CONFIG_FILE src/test/testconfig.sh
else
	cp src/test/testconfig.sh.example src/test/testconfig.sh
fi
if [ -f $DAXEMU_CFG_FILE ]; then
	cp $DAXEMU_CFG_FILE src/test/daxemu.cfg
else
	cp src/test/daxemu.cfg.example src/test/daxemu.cfg
fi

make pcheck $PCHECK_OPTS
"

if [ "${BUILD_PACKAGE_CHECK}" != "y" ]
then
	CHECK_CMD=""
fi

PACKAGE_SOURCE=${PACKAGE_NAME}-${PACKAGE_VERSION}
SOURCE=$PACKAGE_NAME
PACKAGE_TARBALL=$PACKAGE_SOURCE.tar.gz
RPM_SPEC_FILE=$PACKAGE_SOURCE/$PACKAGE_NAME.spec
CHANGELOG_FILE=$PACKAGE_SOURCE/ChangeLog
MAGIC_INSTALL=$PACKAGE_SOURCE/utils/magic-install.sh
MAGIC_UNINSTALL=$PACKAGE_SOURCE/utils/magic-uninstall.sh
OLDPWD=$PWD
OBJ_CPP_NAME=libpmemobj++-devel
OBJ_CPP_DOC_DIR=${OBJ_CPP_NAME}-${PACKAGE_VERSION}

[ -d $WORKING_DIR ] || mkdir -v $WORKING_DIR
[ -d $OUT_DIR ] || mkdir $OUT_DIR

cd $WORKING_DIR

check_dir $SOURCE

mv $SOURCE $PACKAGE_SOURCE

#
# Create parametrized spec file required by rpmbuild.
# Most of variables are set in config.sh file in order to
# keep descriptive values separately from this script.
#
cat << EOF > $RPM_SPEC_FILE

%if 0%{?suse_version} != 0
%define package_group System
%define dist .suse%{suse_version}
%else
%define package_group System Environment
%endif

Name:		$PACKAGE_NAME
Version:	$PACKAGE_VERSION
Release:	$PACKAGE_RELEASE%{?dist}
Summary:	$PACKAGE_SUMMARY
Packager:	$PACKAGE_MAINTAINER
Group:		%{package_group}/Libraries
License:	Intel BSD
URL:		$PACKAGE_URL

Source0:	$PACKAGE_TARBALL
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:	gcc glibc-devel
BuildRequires:	autoconf, automake, make, doxygen
BuildArch:	x86_64

%description
$PACKAGE_DESCRIPTION

%package -n libpmem
Summary: libpmem library
Group: %{package_group}/Libraries
%description -n libpmem
NVML libpmem library

%files -n libpmem
%defattr(-,root,root,-)
%{_libdir}/libpmem.so.*
%{_datadir}/nvml/nvml.magic

%post -n libpmem
$(cat $MAGIC_INSTALL | sed '/^#/d')

%preun -n libpmem
$(cat $MAGIC_UNINSTALL | sed '/^#/d')

%package -n libpmem-devel
Summary: libpmem development library
Group: Development/Libraries
Requires: libpmem >= $PACKAGE_VERSION
%description -n libpmem-devel
Development files for NVML libpmem library

%files -n libpmem-devel
%defattr(-,root,root,-)
%{_libdir}/libpmem.so
%{_libdir}/libpmem.a
%{_libdir}/pkgconfig/libpmem.pc
%{_libdir}/nvml_debug/libpmem.so
%{_libdir}/nvml_debug/libpmem.so.*
%{_libdir}/nvml_debug/libpmem.a
%{_includedir}/libpmem.h
%{_mandir}/man3/libpmem.3.gz

%package -n libpmemblk
Summary: libpmemblk library
Group: %{package_group}/Libraries
%description -n libpmemblk
NVML libpmemblk library

%files -n libpmemblk
%defattr(-,root,root,-)
%{_libdir}/libpmemblk.so.*

%package -n libpmemblk-devel
Summary: libpmemblk development library
Group: Development/Libraries
Requires: libpmemblk >= $PACKAGE_VERSION
%description -n libpmemblk-devel
Development files for NVML libpmemblk library

%files -n libpmemblk-devel
%defattr(-,root,root,-)
%{_libdir}/libpmemblk.so
%{_libdir}/libpmemblk.a
%{_libdir}/pkgconfig/libpmemblk.pc
%{_libdir}/nvml_debug/libpmemblk.so
%{_libdir}/nvml_debug/libpmemblk.so.*
%{_libdir}/nvml_debug/libpmemblk.a
%{_includedir}/libpmemblk.h
%{_mandir}/man3/libpmemblk.3.gz

%package -n libpmemlog
Summary: libpmemlog library
Group: %{package_group}/Libraries
%description -n libpmemlog
NVML libpmemlog library

%files -n libpmemlog
%defattr(-,root,root,-)
%{_libdir}/libpmemlog.so.*

%package -n libpmemlog-devel
Summary: libpmemlog development library
Group: Development/Libraries
Requires: libpmemlog >= $PACKAGE_VERSION
%description -n libpmemlog-devel
Development files for NVML libpmemlog library

%files -n libpmemlog-devel
%defattr(-,root,root,-)
%{_libdir}/libpmemlog.so
%{_libdir}/libpmemlog.a
%{_libdir}/pkgconfig/libpmemlog.pc
%{_libdir}/nvml_debug/libpmemlog.so
%{_libdir}/nvml_debug/libpmemlog.so.*
%{_libdir}/nvml_debug/libpmemlog.a
%{_includedir}/libpmemlog.h
%{_mandir}/man3/libpmemlog.3.gz

%package -n libpmemobj
Summary: libpmemobj library
Group: %{package_group}/Libraries
%description -n libpmemobj
NVML libpmemobj library

%files -n libpmemobj
%defattr(-,root,root,-)
%{_libdir}/libpmemobj.so.*

%package -n libpmemobj-devel
Summary: libpmemobj development library
Group: Development/Libraries
Requires: libpmemobj >= $PACKAGE_VERSION
%description -n libpmemobj-devel
Development files for NVML libpmemobj library

%files -n libpmemobj-devel
%defattr(-,root,root,-)
%{_libdir}/libpmemobj.so
%{_libdir}/libpmemobj.a
%{_libdir}/pkgconfig/libpmemobj.pc
%{_libdir}/nvml_debug/libpmemobj.so
%{_libdir}/nvml_debug/libpmemobj.so.*
%{_libdir}/nvml_debug/libpmemobj.a
%{_includedir}/libpmemobj.h
%{_includedir}/libpmemobj/*.h
%{_mandir}/man3/libpmemobj.3.gz

%package -n libpmempool
Summary: libpmempool library
Group: %{package_group}/Libraries
%description -n libpmempool
NVML libpmempool library

%files -n libpmempool
%defattr(-,root,root,-)
%{_libdir}/libpmempool.so.*

%package -n libpmempool-devel
Summary: libpmempool development library
Group: Development/Libraries
Requires: libpmempool >= $PACKAGE_VERSION
%description -n libpmempool-devel
Development files for NVML libpmempool library

%files -n libpmempool-devel
%defattr(-,root,root,-)
%{_libdir}/libpmempool.so
%{_libdir}/libpmempool.a
%{_libdir}/pkgconfig/libpmempool.pc
%{_libdir}/nvml_debug/libpmempool.so
%{_libdir}/nvml_debug/libpmempool.so.*
%{_libdir}/nvml_debug/libpmempool.a
%{_includedir}/libpmempool.h
%{_mandir}/man3/libpmempool.3.gz

%package -n libvmem
Summary: libvmem library
Group: %{package_group}/Libraries
%description -n libvmem
NVML libvmem library

%files -n libvmem
%defattr(-,root,root,-)
%{_libdir}/libvmem.so.*

%package -n libvmem-devel
Summary: libvmem development library
Group: Development/Libraries
Requires: libvmem >= $PACKAGE_VERSION
%description -n libvmem-devel
Development files for NVML libvmem library

%files -n libvmem-devel
%defattr(-,root,root,-)
%{_libdir}/libvmem.so
%{_libdir}/libvmem.a
%{_libdir}/pkgconfig/libvmem.pc
%{_libdir}/nvml_debug/libvmem.so
%{_libdir}/nvml_debug/libvmem.so.*
%{_libdir}/nvml_debug/libvmem.a
%{_includedir}/libvmem.h
%{_mandir}/man3/libvmem.3.gz

%package -n libvmmalloc
Summary: libvmmalloc library
Group: %{package_group}/Libraries
%description -n libvmmalloc
NVML libvmmalloc library

%files -n libvmmalloc
%defattr(-,root,root,-)
%{_libdir}/libvmmalloc.so.*

%package -n libvmmalloc-devel
Summary: libvmmalloc development library
Group: Development/Libraries
Requires: libvmmalloc >= $PACKAGE_VERSION
%description -n libvmmalloc-devel
Development files for NVML libvmmalloc library

%files -n libvmmalloc-devel
%defattr(-,root,root,-)
%{_libdir}/libvmmalloc.so
%{_libdir}/libvmmalloc.a
%{_libdir}/pkgconfig/libvmmalloc.pc
%{_libdir}/nvml_debug/libvmmalloc.so
%{_libdir}/nvml_debug/libvmmalloc.so.*
%{_libdir}/nvml_debug/libvmmalloc.a
%{_includedir}/libvmmalloc.h
%{_mandir}/man3/libvmmalloc.3.gz

%package -n ${OBJ_CPP_NAME}
Summary: C++ bindings for libpmemobj
Group: Development/Libraries
Requires: libpmemobj-devel = %{version}
%description -n ${OBJ_CPP_NAME}
Development files for NVML C++ libpmemobj bindings

%files -n ${OBJ_CPP_NAME}
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/libpmemobj++.pc
%{_includedir}/libpmemobj++/*.hpp
%{_includedir}/libpmemobj++/detail/*.hpp
%{_docdir}/${OBJ_CPP_NAME}-%{version}/*

%package tools
Group:		%{package_group}
Summary:	Tools for %{name}

%description tools
Useful applications for administration and diagnostic purposes.

%files tools
%{_bindir}/pmempool
%{_mandir}/man1/pmempool.1.gz
%{_mandir}/man1/pmempool-info.1.gz
%{_mandir}/man1/pmempool-create.1.gz
%{_mandir}/man1/pmempool-dump.1.gz
%{_mandir}/man1/pmempool-check.1.gz
%{_mandir}/man1/pmempool-rm.1.gz
%{_mandir}/man1/pmempool-convert.1.gz
%{_mandir}/man1/pmempool-sync.1.gz
%{_mandir}/man1/pmempool-transform.1.gz
%{_sysconfdir}/bash_completion.d/pmempool.sh

%prep
%setup -q -n $PACKAGE_SOURCE

%build
if [ -f $TEST_CONFIG_FILE ]; then
	cp $TEST_CONFIG_FILE src/test/testconfig.sh
else
	cp src/test/testconfig.sh.example src/test/testconfig.sh
fi
%{__make}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}\
	prefix=%{_prefix}\
	libdir=%{_libdir}\
	includedir=%{_includedir}\
	mandir=%{_mandir}\
	bindir=%{_bindir}\
	sysconfdir=%{_sysconfdir}\
	docdir=%{_docdir}\
	EXPERIMENTAL=${EXPERIMENTAL}\
	CPP_DOC_DIR=${OBJ_CPP_DOC_DIR}\
	NORPATH=1
mkdir -p %{buildroot}%{_datadir}/nvml
cp utils/nvml.magic %{buildroot}%{_datadir}/nvml/

${CHECK_CMD}

%clean
make clobber

%if 0%{?__debug_package} == 0
%debug_package
%endif

EOF

# Experimental features
if [ "${EXPERIMENTAL}" = "y" ]
then
	echo "no experimental features for now"
fi

# librpmem & rpmemd
if [ "${BUILD_RPMEM}" = "y" ]
then
	add_rpmem_packages;
fi

[ -f $CHANGELOG_FILE ] && create_changelog $CHANGELOG_FILE >> $RPM_SPEC_FILE

tar zcf $PACKAGE_TARBALL $PACKAGE_SOURCE

# Create directory structure for rpmbuild
mkdir -v BUILD SPECS

rpmbuild --define "_topdir `pwd`"\
         --define "_rpmdir ${OUT_DIR}"\
	 --define "_srcrpmdir ${OUT_DIR}"\
         -ta $PACKAGE_TARBALL

echo "Building rpm packages done"

exit 0
