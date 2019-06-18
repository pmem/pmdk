# **PMDK: Persistent Memory Development Kit**

[![Build Status](https://travis-ci.org/pmem/pmdk.svg?branch=master)](https://travis-ci.org/pmem/pmdk)
[![Build status](https://ci.appveyor.com/api/projects/status/u2l1db7ucl5ktq10/branch/master?svg=true&pr=false)](https://ci.appveyor.com/project/pmem/pmdk/branch/master)
[![Build status](https://api.cirrus-ci.com/github/pmem/pmdk.svg)](https://cirrus-ci.com/github/pmem/pmdk/master)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/3015.svg)](https://scan.coverity.com/projects/pmem-pmdk)
[![PMDK release version](https://img.shields.io/github/release/pmem/pmdk.svg)](https://github.com/pmem/pmdk/releases/latest)
[![Coverage Status](https://codecov.io/github/pmem/pmdk/coverage.svg?branch=master)](https://codecov.io/gh/pmem/pmdk/branch/master)

The **Persistent Memory Development Kit (PMDK)** is a collection of libraries and tools for System Administrators and Application Developers to simplify managing and accessing persistent memory devices. For more information, see http://pmem.io.

To install PMDK libraries, either install pre-built packages, which we build for every stable release, or clone the tree and build it yourself. **Pre-built** packages can be found in popular Linux distribution package repositories, or you can check out our recent stable releases on our [github release page](https://github.com/pmem/pmdk/releases). Specific installation instructions are outlined below.

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
6. [Building PMDK on Linux or FreeBSD](#building-pmdk-on-linux-or-freebsd)
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
- [libpmem](http://pmem.io/pmdk/libpmem/):  provides low level persistent memory support

- [libpmemobj](http://pmem.io/pmdk/libpmemobj/):  provides a transactional object store, providing memory allocation, transactions, and general facilities for persistent memory programming.

- [libpmemblk](http://pmem.io/pmdk/libpmemblk/):  supports arrays of pmem-resident blocks, all the same size, that are atomically updated.

- [libpmemlog](http://pmem.io/pmdk/libpmemlog/):  provides a pmem-resident log file.

- [libvmem](http://pmem.io/pmdk/libvmem/):  turns a pool of persistent memory into a volatile memory pool, similar to the system heap but kept separate and with its own malloc-style API. It is not actively developed and is deprecated -- we recommended to use [libmemkind](https://github.com/memkind/memkind) in new code instead.

- [libvmmalloc](http://pmem.io/pmdk/libvmmalloc/)<sup>1</sup>:  transparently converts all the dynamic memory allocations into persistent memory allocations. Just like libvmem, it is no longer actively developed and is deprecated.

- [libpmempool](http://pmem.io/pmdk/libpmempool/):  provides support for off-line pool management and diagnostics.

- [librpmem](http://pmem.io/pmdk/librpmem/)<sup>1</sup>:  provides low-level support for remote access to persistent memory utilizing RDMA-capable RNICs.

Available Utilities:

- [pmempool](http://pmem.io/pmdk/pmempool/): Manage and analyze persistent memory pools with this stand-alone utility

- [pmemcheck](http://pmem.io/2015/07/17/pmemcheck-basic.html): Use dynamic runtime analysis with an enhanced version of Valgrind for use with persistent memory.

Currently these libraries only work on 64-bit Linux, Windows<sup>2</sup>, and 64-bit FreeBSD 11+<sup>3</sup>.
For information on how these libraries are licensed, see our [LICENSE](LICENSE) file.

><sup>1</sup> Not supported on Windows.
>
><sup>2</sup> PMDK for Windows is feature complete, but not yet considered production quality.
>
><sup>3</sup> DAX and **libfabric** are not yet supported in FreeBSD, so at this time PMDK is available as a technical preview release for development purposes.

## Getting Started

Getting Started with Persistent Memory Programming is a tutorial series created by Intel Architect, Andy Rudoff. In this tutorial, you will be introduced to persistent memory programming and learn how to apply it to your applications.
- Part 1: [What is Persistent Memory?](https://software.intel.com/en-us/persistent-memory/get-started/series)
- Part 2: [Describing The SNIA Programming Model](https://software.intel.com/en-us/videos/the-nvm-programming-model-persistent-memory-programming-series)
- Part 3: [Introduction to PMDK Libraries](https://software.intel.com/en-us/videos/intro-to-the-nvm-libraries-persistent-memory-programming-series)
- Part 4: [Thinking Transactionally](https://software.intel.com/en-us/videos/thinking-transactionally-persistent-memory-programming-series)
- Part 5: [A C++ Example](https://software.intel.com/en-us/videos/a-c-example-persistent-memory-programming-series)


Additionally, we recommend reading [Introduction to Programming with Persistent Memory from Intel](https://software.intel.com/en-us/articles/introduction-to-programming-with-persistent-memory-from-intel)


## Version Conventions

- **Builds** are tagged something like `0.2+b1`, which means _Build 1 on top of version 0.2_
- **Release Candidates** have a '-rc{version}' tag, e.g. `0.2-rc3, meaning _Release Candidate 3 for version 0.2_
- **Stable Releases** use a _major.minor_ tag like `0.2`

## Pre-Built Packages for Windows

The recommended and easiest way to install PMDK on Windows is to use Microsoft vcpkg. Vcpkg is an open source tool and ecosystem created for library management.

To install the latest PMDK release and link it to your Visual Studio solution you first need to clone and set up vcpkg on your machine as described on the [vcpkg github page](https://github.com/Microsoft/vcpkg) in **Quick Start** section.

In brief:

```
	> git clone https://github.com/Microsoft/vcpkg
	> cd vcpkg
	> .\bootstrap-vcpkg.bat
	> .\vcpkg integrate install
	> .\vcpkg install pmdk:x64-windows
```

The last command can take a while - it is PMDK building and installation time.

After a successful completion of all of the above steps, the libraries are ready
to be used in Visual Studio and no additional configuration is required.
Just open VS with your already existing project or create a new one
(remember to use platform **x64**) and then include headers to project as you always do.

## Dependencies

Required packages for each supported OS are listed below. It is important to note that some tests and example applications require additional packages, but they do not interrupt building if they are missing. An appropriate message is displayed instead. For details please read the DEPENDENCIES section in the appropriate README file.

See our **[Dockerfiles](utils/docker/images)**
to get an idea what packages are required to build the entire PMDK,
with all the tests and examples on the _Travis-CI_ system.

### Linux

You will need to install the following required packages on the build system:

* **autoconf**
* **pkg-config**
* **libndctl-devel** (v60.1 or later)<sup>1</sup>
* **libdaxctl-devel** (v60.1 or later)

The following packages are required only by selected PMDK components
or features:

* **libfabric** (v1.4.2 or later) -- required by **librpmem**

><sup>1</sup> PMDK requires libndctl v63 or newer (to enable RAS features) by default. If you have such libndctl version but you don't want to depend on this API please bulid PMDK with NDCTL_USE_V63_API=n. For more information please check this issue pmem/issues#1039

### Windows

* **MS Visual Studio 2015**
* [Windows SDK 10.0.16299.15](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)
* **perl** (i.e. [StrawberryPerl](http://strawberryperl.com/))
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


## Building PMDK on Linux or FreeBSD

To build from source, clone this tree:
```
	$ git clone https://github.com/pmem/pmdk
	$ cd pmdk
```

For a stable version, checkout a [release tag](https://github.com/pmem/pmdk/releases) as follows. Otherwise skip this step to build the latest development release.
```
	$ git checkout tags/1.6
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

The PMDK libraries support standard Valgrind DRD, Helgrind and Memcheck, as well as a PM-aware version of [Valgrind](https://github.com/pmem/valgrind) (not yet available for FreeBSD). By default, support for all tools is enabled. If you wish to disable it, supply the compiler with **VG_\<TOOL\>_ENABLED** flag set to 0, for example:
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

## Building PMDK on Windows

Clone the PMDK tree and open the solution:
```
	> git clone https://github.com/pmem/pmdk
	> cd pmdk/src
	> devenv PMDK.sln
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

## Experimental Packages

Some components in the source tree are treated as experimental. By default,
those components are built but not installed (and thus not included in
packages).

If you want to build/install experimental packages run:
```
	$ make EXPERIMENTAL=y [install,rpm,dpkg]
```

### The librpmem and rpmemd packages

**NOTE:**
The **libfabric** package required to build the **librpmem** and **rpmemd** is
not yet available on stable Debian-based distributions. This makes it
impossible to create Debian packages.

If you want to build Debian packages of **librpmem** and **rpmemd** run:
```
	$ make RPMEM_DPKG=y dpkg
```

### Experimental Support for 64-bit ARM

There is an initial support for 64-bit ARM processors provided,
currently only for aarch64.  All the PMDK libraries except **librpmem**
can be built for 64-bit ARM.  The examples, tools and benchmarks
are not ported yet and may not get built on ARM cores.

**NOTE:**
The support for ARM processors is highly experimental. The libraries
are only validated to "early access" quality with Cortex-A53 processor.

## Contact Us

For more information on this library, contact
Marcin Slusarz (marcin.slusarz@intel.com),
Andy Rudoff (andy.rudoff@intel.com), or post to our
[Google group](http://groups.google.com/group/pmem).
