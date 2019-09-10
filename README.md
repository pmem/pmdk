# **VMDK: Volatile Memory Development Kit**

[![Build Status](https://travis-ci.org/pmem/pmdk.svg?branch=master)](https://travis-ci.org/pmem/pmdk)
[![Build status](https://ci.appveyor.com/api/projects/status/u2l1db7ucl5ktq10/branch/master?svg=true&pr=false)](https://ci.appveyor.com/project/pmem/pmdk/branch/master)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/3015.svg)](https://scan.coverity.com/projects/pmem-pmdk)
[![PMDK release version](https://img.shields.io/github/release/pmem/pmdk.svg)](https://github.com/pmem/pmdk/releases/latest)
[![Coverage Status](https://codecov.io/github/pmem/pmdk/coverage.svg?branch=master)](https://codecov.io/gh/pmem/pmdk/branch/master)

The **Volatile Memory Development Kit (VMDK)** are a couple of libraries for
using persistent memory for malloc-like volatile uses.  They have
historically been a part of [PMDK](https://pmem.io/pmdk) despite being
solely for volatile uses.

Both of these libraries are considered code-complete and mature.  You may
want consider using [memkind](https://github.com/memkind/memkind) in code
that benefits from extra features like NUMA awareness.

To install VMDK libraries, either install pre-built packages, which we build for every stable release, or clone the tree and build it yourself. **Pre-built** packages can be found in popular Linux distribution package repositories, or you can check out our recent stable releases on our [github release page](https://github.com/pmem/pmdk/releases). Specific installation instructions are outlined below.

Bugs and feature requests for this repo are tracked in our [GitHub Issues Database](https://github.com/pmem/issues/issues).

## Contents
1. [Libraries and Utilities](#libraries-and-utilities)
2. [Getting Started](#getting-started)
3. [Version Conventions](#version-conventions)
4. [Pre-Built Packages for Windows](#pre-built-packages-for-windows)
5. [Dependencies](#dependencies)
	* [Linux](#linux)
	* [Windows](#windows)
	* [FreeBSD](#freebsd)
6. [Building VMDK on Linux or FreeBSD](#building-pmdk-on-linux-or-freebsd)
	* [Make Options](#make-options)
	* [Testing Libraries](#testing-libraries-on-linux-and-freebsd)
	* [Memory Management Tools](#memory-management-tools)
7. [Building PMDK on Windows](#building-pmdk-on-windows)
	* [Testing Libraries](#testing-libraries-on-windows)
8. [Experimental Packages](#experimental-packages)
	* [librpmem and rpmemd packages](#the-librpmem-and-rpmemd-packages)
	* [Experimental support for 64-bit ARM](#experimental-support-for-64-bit-arm)
9. [Contact Us](#contact-us)

## Libraries and Utilities
Available Libraries:
- [libvmem](http://pmem.io/pmdk/libvmem/):  turns a pool of persistent memory into a volatile memory pool, similar to the system heap but kept separate and with its own malloc-style API.

- [libvmmalloc](http://pmem.io/pmdk/libvmmalloc/)<sup>1</sup>:  transparently converts all the dynamic memory allocations into persistent memory allocations.

Currently these libraries only work on 64-bit Linux, Windows<sup>2</sup>, and 64-bit FreeBSD 11+<sup>3</sup>.
For information on how these libraries are licensed, see our [LICENSE](LICENSE) file.

><sup>1</sup> Not supported on Windows.
>
><sup>2</sup> VMDK for Windows is feature complete, but not yet considered production quality.
>
><sup>3</sup> DAX is not yet supported in FreeBSD, so at this time VMDK is available as a technical preview release for development purposes.


## Pre-Built Packages for Windows

The recommended and easiest way to install VMDK on Windows is to use Microsoft vcpkg. Vcpkg is an open source tool and ecosystem created for library management.

To install the latest VMDK release and link it to your Visual Studio solution you first need to clone and set up vcpkg on your machine as described on the [vcpkg github page](https://github.com/Microsoft/vcpkg) in **Quick Start** section.

In brief:

```
	> git clone https://github.com/Microsoft/vcpkg
	> cd vcpkg
	> .\bootstrap-vcpkg.bat
	> .\vcpkg integrate install
	> .\vcpkg install pmdk:x64-windows
```

The last command can take a while - it is VMDK building and installation time.

After a successful completion of all of the above steps, the libraries are ready
to be used in Visual Studio and no additional configuration is required.
Just open VS with your already existing project or create a new one
(remember to use platform **x64**) and then include headers to project as you always do.

## Dependencies

Required packages for each supported OS are listed below. It is important to note that some tests and example applications require additional packages, but they do not interrupt building if they are missing. An appropriate message is displayed instead. For details please read the DEPENDENCIES section in the appropriate README file.

See our **[Dockerfiles](utils/docker/images)**
to get an idea what packages are required to build the entire VMDK,
with all the tests and examples on the _Travis-CI_ system.

### Linux

You will need to install the following required packages on the build system:

* **autoconf**
* **pkg-config**

### Windows

* **MS Visual Studio 2015**
* [Windows SDK 10.0.16299.15](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)
* **perl** (i.e. [ActivePerl](http://www.activestate.com/activeperl/downloads))
* **PowerShell 5**

### FreeBSD

* **autoconf**
* **bash**
* **binutils**
* **coreutils**
* **e2fsprogs-libuuid**
* **gmake**
* **libunwind**
* **ncurses**<sup>4</sup>
* **pkgconf**

><sup>4</sup> The pkg version of ncurses is required for proper operation; the base version included in FreeBSD is not sufficient.


## Building VMDK on Linux or FreeBSD

To build from source, clone this tree:
```
	$ git clone https://github.com/pmem/pmdk
	$ cd pmdk
```

For a stable version, checkout a [release tag](https://github.com/pmem/pmdk/releases) as follows. Otherwise skip this step to build the latest development release.
```
	$ git checkout tags/1.4.2
```

Once the build system is setup, the Persistent Memory Development Kit is built using the `make` command at the top level:
```
	$ make
```
For FreeBSD, use `gmake` rather than `make`.

By default, all code is built with the `-Werror` flag, which fails
the whole build when the compiler emits any warning. This is very useful during
development, but can be annoying in deployment. If you want to **disable -Werror**,
use the EXTRA_CFLAGS variable:
```
	$ make EXTRA_CFLAGS="-Wno-error"
```
>or
```
	$ make EXTRA_CFLAGS="-Wno-error=$(type-of-warning)"
```

### Make Options

There are many options that follow `make`. If you want to invoke make with the same variables multiple times, you can create a user.mk file in the top level directory and put all variables there.
For example:
```
	$ cat user.mk
	EXTRA_CFLAGS_RELEASE = -ggdb -fno-omit-frame-pointer
	PATH += :$HOME/valgrind/bin
```
This feature is intended to be used only by developers and it may not work for all variables. Please do not file bug reports about it. Just fix it and make a PR.

**Built-in tests:** can be compiled and ran with different compiler. To do this, you must provide the `CC` and `CXX` variables. These variables are independent and setting `CC=clang` does not set `CXX=clang++`.
For example:
```
	$ make CC=clang CXX=clang++
```
Once make completes, all the libraries and examples are built. You can play with the library within the build tree, or install it locally on your machine. For information about running different types of tests, please refer to the [src/test/README](src/test/README).


**Installing the library** is convenient since it installs man pages and libraries in the standard system locations:
```
	(as root...)
	# make install
```

To install this library into **other locations**, you can use the `prefix` variable, e.g.:
```
	$ make install prefix=/usr/local
```
This will install files to /usr/local/lib, /usr/local/include /usr/local/share/man.

**Prepare library for packaging** can be done using the DESTDIR variable, e.g.:
```
	$ make install DESTDIR=/tmp
```
This will install files to /tmp/usr/lib, /tmp/usr/include /tmp/usr/share/man.

**Man pages** (groff files) are generated as part of the `install` rule. To generate the documentation separately, run:
```
	$ make doc
```
This call requires the following dependencies: **pandoc**. Pandoc is provided by the hs-pandoc package on FreeBSD.


**Install copy of source tree** can be done by specifying the path where you want it installed.
```
	$ make source DESTDIR=some_path
```
For this example, it will be installed at $(DESTDIR)/pmdk.

**Build rpm packages** on rpm-based distributions is done by:
```
	$ make rpm
```

To build rpm packages without running tests:
```
	$ make BUILD_PACKAGE_CHECK=n rpm
```
This requires **rpmbuild** to be installed.


**Build dpkg packages** on Debian-based distributions is done by:
```
	$ make dpkg
```

To build dpkg packages without running tests:
```
	$ make BUILD_PACKAGE_CHECK=n dpkg
```
This requires **devscripts** to be installed.

### Testing Libraries on Linux and FreeBSD

Before running the tests, you may need to prepare a test configuration file (src/test/testconfig.sh). Please see the available configuration settings in the example file [src/test/testconfig.sh.example](src/test/testconfig.sh.example).

To build and run the **unit tests**:
```
	$ make check
```

To run a specific **subset of tests**, run for example:
```
	$ make check TEST_TYPE=short TEST_BUILD=debug TEST_FS=pmem
```

To **modify the timeout** which is available for **check** type tests, run:
```
	$ make check TEST_TIME=1m
```
This will set the timeout to 1 minute.

Please refer to the **src/test/README** for more details on how to
run different types of tests.

### Memory Management Tools

The VMDK libraries support standard Valgrind DRD, Helgrind and Memcheck, as well as a PM-aware version of [Valgrind](https://github.com/pmem/valgrind) (not yet available for FreeBSD). By default, support for all tools is enabled. If you wish to disable it, supply the compiler with **VG_\<TOOL\>_ENABLED** flag set to 0, for example:
```
	$ make EXTRA_CFLAGS=-DVG_MEMCHECK_ENABLED=0
```

**VALGRIND_ENABLED** flag, when set to 0, disables all Valgrind tools
(drd, helgrind, memcheck and pmemcheck).

The **SANITIZE** flag allows the libraries to be tested with various
sanitizers. For example, to test the libraries with AddressSanitizer
and UndefinedBehaviorSanitizer, run:
```
	$ make SANITIZE=address,undefined clobber check
```

The address sanitizer is not supported for libvmmalloc on FreeBSD and will be ignored.

## Building VMDK on Windows

Clone the VMDK tree and open the solution:
```
	> git clone https://github.com/pmem/pmdk
	> cd pmdk/src
	> devenv VMDK.sln
```

Select the desired configuration (Debug or Release) and build the solution
(i.e. by pressing Ctrl-Shift-B).

### Testing Libraries on Windows

Before running the tests, you may need to prepare a test configuration file (src/test/testconfig.ps1). Please see the available configuration settings in the example file [src/test/testconfig.ps1.example](src/test/testconfig.ps1.example).

To **run the unit tests**, open the PowerShell console and type:
```
	> cd pmdk/src/test
	> RUNTESTS.ps1
```

To run a specific **subset of tests**, run for example:
```
	> RUNTESTS.ps1 -b debug -t short
```

To run **just one test**, run for example:
```
	> RUNTESTS.ps1 -b debug -i pmem_is_pmem
```

To **modify the timeout**, run:
```
	> RUNTESTS.ps1 -o 3m
```
This will set the timeout to 3 minutes.

To **display all the possible options**, run:
```
	> RUNTESTS.ps1 -h
```

Please refer to the **[src/test/README](src/test/README)** for more details on how to run different types of tests.

### Experimental Support for 64-bit non-x86 architectures

There's generally no architecture-specific parts anywhere in these
libraries, but they have received no real testing outside of 64-bit x86.

## Contact Us

For more information on this library, contact
Marcin Slusarz (marcin.slusarz@intel.com),
Andy Rudoff (andy.rudoff@intel.com), or post to our
[Google group](http://groups.google.com/group/pmem).
