# **libvmem and libvmmalloc: malloc-like volatile allocations**

**libvmem** and **libvmmalloc** are a couple of libraries for
using persistent memory for malloc-like volatile uses.  They have
historically been a part of [PMDK](https://pmem.io/pmdk) despite being
solely for volatile uses.

Both of these libraries are considered code-complete and mature.  You may
want consider using [memkind](https://github.com/memkind/memkind) instead
in code that benefits from extra features like NUMA awareness.

To install vmem libraries, either install pre-built packages, which we build
for every stable release, or clone the tree and build it yourself.
**Pre-built** packages can be found in popular Linux distribution package
repositories, or you can check out our recent stable releases on our [github
release page](https://github.com/pmem/vmem/releases).  Specific installation
instructions are outlined below.

## Contents
1. [Libraries](#libraries)
2. [Getting Started](#getting-started)
3. [Version Conventions](#version-conventions)
4. [Pre-Built Packages for Windows](#pre-built-packages-for-windows)
5. [Dependencies](#dependencies)
	* [Linux](#linux)
	* [Windows](#windows)
	* [FreeBSD](#freebsd)
6. [Building vmem on Linux or FreeBSD](#building-vmem-on-linux-or-freebsd)
	* [Make Options](#make-options)
	* [Testing Libraries](#testing-libraries-on-linux-and-freebsd)
	* [Memory Management Tools](#memory-management-tools)
7. [Building vmem on Windows](#building-vmem-on-windows)
	* [Testing Libraries](#testing-libraries-on-windows)
8. [Experimental Packages](#experimental-packages)
	* [Experimental support for 64-bit ARM](#experimental-support-for-64-bit-arm)
9. [Contact Us](#contact-us)

## Libraries
Available Libraries:
- [libvmem](http://pmem.io/pmdk/libvmem/):  turns a pool of persistent memory into a volatile memory pool, similar to the system heap but kept separate and with its own malloc-style API.

- [libvmmalloc](http://pmem.io/pmdk/libvmmalloc/)<sup>1</sup>:  transparently converts all the dynamic memory allocations into persistent memory allocations.

Currently these libraries only work on 64-bit Linux, Windows<sup>2</sup>, and 64-bit FreeBSD 11+.
For information on how these libraries are licensed, see our [LICENSE](LICENSE) file.

><sup>1</sup> Not supported on Windows.
>
><sup>2</sup> VMEM for Windows is feature complete, but not yet considered production quality.


## Pre-Built Packages for Windows

The recommended and easiest way to install VMEM on Windows is to use Microsoft vcpkg. Vcpkg is an open source tool and ecosystem created for library management.

To install the latest VMEM release and link it to your Visual Studio solution you first need to clone and set up vcpkg on your machine as described on the [vcpkg github page](https://github.com/Microsoft/vcpkg) in **Quick Start** section.

In brief:

```
	> git clone https://github.com/Microsoft/vcpkg
	> cd vcpkg
	> .\bootstrap-vcpkg.bat
	> .\vcpkg integrate install
	> .\vcpkg install vmem:x64-windows
```

The last command can take a while - it is VMEM building and installation time.

After a successful completion of all of the above steps, the libraries are ready
to be used in Visual Studio and no additional configuration is required.
Just open VS with your already existing project or create a new one
(remember to use platform **x64**) and then include headers to project as you always do.

## Dependencies

Required packages for each supported OS are listed below.

### Linux

You will need to install the following required packages on the build system:

* **autoconf**
* **pkg-config**

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


## Building vmem on Linux or FreeBSD

To build from source, clone this tree:
```
	$ git clone https://github.com/pmem/vmem
	$ cd vmem
```

For a stable version, checkout a [release tag](https://github.com/pmem/vmem/releases) as follows. Otherwise skip this step to build the latest development release.
```
	$ git checkout tags/1.6.1
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

## Building vmem on Windows

Clone the vmem tree and open the solution:
```
	> git clone https://github.com/pmem/vmem
	> cd vmem/src
	> devenv VMEM.sln
```

Select the desired configuration (Debug or Release) and build the solution
(i.e. by pressing Ctrl-Shift-B).

### Testing Libraries on Windows

Before running the tests, you may need to prepare a test configuration file (src/test/testconfig.ps1). Please see the available configuration settings in the example file [src/test/testconfig.ps1.example](src/test/testconfig.ps1.example).

To **run the unit tests**, open the PowerShell console and type:
```
	> cd vmem/src/test
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
