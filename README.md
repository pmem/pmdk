nvml: Non-Volatile Memory Library
=================================

[![Build Status](https://travis-ci.org/pmem/nvml.svg?branch=master)](https://travis-ci.org/pmem/nvml)
[![Build status](https://ci.appveyor.com/api/projects/status/sehrom4f1neihucf/branch/master?svg=true&pr=false)](https://ci.appveyor.com/project/pmem/nvml/branch/master)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/3015.svg)](https://scan.coverity.com/projects/pmem-nvml)
[![NVML release version](https://img.shields.io/github/release/pmem/nvml.svg)](https://github.com/pmem/nvml/releases/latest)

This is the top-level README.md of the NVM Library.
For more information, see http://pmem.io.

### The Libraries ###

Please see the file [LICENSE](https://github.com/pmem/nvml/blob/master/LICENSE)
for information on how this library is licensed.

This tree contains a collection of libraries for using Non-Volatile Memory
(NVM).  There are currently eight libraries:

* **libpmem** -- basic pmem operations like flushing
* **libpmemblk**, **libpmemlog**, **libpmemobj** -- pmem transactions
* **libvmem**, **libvmmalloc** -- volatile use of pmem
* **libpmempool** -- persistent memory pool management
* **librpmem** -- remote access to persistent memory (EXPERIMENTAL)

and one command-line utility:

* **pmempool** -- standalone tool for off-line pool management

These libraries and utilities are described in more detail on the
[pmem web site](http://pmem.io).  There you'll find man pages, examples,
and tutorials.

**Currently, these libraries only work on 64-bit Linux and Windows (\*).**

>(\*) **NOTE: Porting NVML to Windows is still in progress.**
>
>The source tree contains MS Visual Studio solution and project files,
allowing to compile _libpmem_, _libpmemlog_, _libpmemblk_, _libpmemobj_,
_libpmempool_ and _libvmem_ libraries for Windows, with all the corresponding
unit tests and selected examples.  The _pmempool_ utility and NVML
benchmarks are also ported.  Current progress of this work is tracked on
[NVML for Windows Trello Board](https://trello.com/b/IMPSJ4Iu/nvml-for-windows).
See also description of the first [NVML for Windows Technical Preview release]
(https://github.com/pmem/nvml/releases/1.2+wtp1)
for the list of known issues and limitations in the current version
of Windows support in NVML.

### Pre-Built Packages ###

If you want to install these libraries to try them out of your system, you can
either install pre-built packages, which we build for every stable release, or
clone the tree and build it yourself.

Builds are tagged something like `0.2+b1`, which means
*Build 1 on top of version 0.2* and `0.2-rc3`, which means
*Release Candidate 3 for version 0.2*.  **Stable** releases
are the simpler *major.minor* tags like `0.2`.  To find
pre-build packages, check the Downloads associated with
the stable releases on the
[github release page](https://github.com/pmem/nvml/releases).

### Building The Source ###

The source tree is organized as follows:

* **doc** -- man pages describing each library contained here
* **src** -- the source for the libraries
* **src/include** -- public header files for all the libraries
* **src/benchmarks** -- benchmarks used by development team
* **src/examples** -- brief example programs using these libraries
* **src/test** -- unit tests used by development team
* **src/tools** -- various tools developed for NVML
* **src/windows** -- Windows-specific source and header files
* **utils** -- utilities used during build & test
* **CONTRIBUTING.md** -- instructions for people wishing to contribute
* **CODING_STYLE.md** -- coding standard and conventions for NVML

To build this library on Linux, you may need to install the following
required packages on the build system:

* **autoconf**
* **pkg-config**


On Windows, to build NVML and run the tests you need:
* **MS Visual Studio 2015**
* [Windows SDK 10.0.14393](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk) (or later)
* **perl** (i.e. [ActivePerl](http://www.activestate.com/activeperl/downloads))


Some tests and example applications require additional packages, but they
do not interrupt building if they are missing. An appropriate message is
displayed instead. For details please read the **DEPENDENCIES** section
in appropriate README file.


See our [Dockerfiles](https://github.com/pmem/nvml/blob/master/utils/docker/images/)
to get an idea what packages are required to build on the _Travis-CI_
system.


#### Building NVML on Linux ####

To build the latest development version, just clone this tree and build
the master branch:
```
	$ git clone https://github.com/pmem/nvml
	$ cd nvml
```

Once the build system is setup, the NVM Library is built using
this command at the top level:
```
	$ make
```

If you want to compile, and hopefully run the builtin tests, with a different
compiler, you have to provide the `CC` and `CXX` variables. For example:
```
	$ make CC=clang CXX=clang++
```

These variables are independent and setting `CC=clang` does not set `CXX=clang++`.

Once the make completes (*), all the libraries are built and the examples
under `src/examples` are built as well.  You can play with the library
within the build tree, or install it locally on your machine.  Installing
the library is more convenient since it installs man pages and libraries
in the standard system locations:
```
	(as root...)
	# make install
```

To install this library into other locations, you can use the
prefix variable, e.g.:
```
	$ make install prefix=/usr/local
```
This will install files to /usr/local/lib, /usr/local/include /usr/local/share/man.

To prepare this library for packaging, you can use the
DESTDIR variable, e.g.:
```
	$ make install DESTDIR=/tmp
```
This will install files to /tmp/usr/lib, /tmp/usr/include /tmp/usr/share/man.

The man pages (groff files) are generated as part of the `install` rule. To
generate the documentation separately, run:
```
	$ make doc
```
**DEPENDENCIES:** pandoc

To install a complete copy of the source tree to $(DESTDIR)/nvml:
```
	$ make source DESTDIR=some_path
```

To build rpm packages on rpm-based distributions:
```
	$ make rpm
```

If you want to build packages without running tests, run:
```
	$ make BUILD_PACKAGE_CHECK=n rpm
```
**DEPENDENCIES:** rpmbuild

To build dpkg packages on Debian-based distributions:
```
	$ make dpkg
```

If you want to build packages without running tests, run:
```
	$ make BUILD_PACKAGE_CHECK=n dpkg
```
**DEPENDENCIES:** devscripts

(*) By default all code is built with -Werror flag which fails the whole build
when compiler emits any warning. It's very useful during development, but can be
annoying in deployment. If you want to disable -Werror, you can use EXTRA_CFLAGS
variable:
```
	$ make EXTRA_CFLAGS="-Wno-error"
```
or
```
	$ make EXTRA_CFLAGS="-Wno-error=$(type-of-warning)"
```

#### Testing the Libraries ####

Before running the tests, you may need to prepare a test configuration file
(src/test/testconfig.sh).  Please see the available configuration settings
in the example file (src/test/testconfig.sh.example).

To build and run the unit tests:
```
	$ make check
```

To run a specific subset of tests, run for example:
```
	$ make check TEST_TYPE=short TEST_BUILD=debug TEST_FS=pmem
```

To modify the timeout which is available for **check** type tests, run:
```
	$ make check TEST_TIME=1m
```
This will set the timeout to 1 minute.

Please refer to the **src/test/README** for more details on how to
run different types of tests.

To compile this library with enabled support for the PM-aware version
of [Valgrind](https://github.com/pmem/valgrind), supply the compiler
with the **USE_VG_PMEMCHECK** flag, for example:
```
	$ make EXTRA_CFLAGS=-DUSE_VG_PMEMCHECK
```
For Valgrind memcheck support, supply **USE_VG_MEMCHECK** flag.
**USE_VALGRIND** flag enables both.

To test the libraries with AddressSanitizer and UndefinedBehaviorSanitizer, run:
```
	$ make EXTRA_CFLAGS="-fsanitize=address,undefined" EXTRA_LDFLAGS="-fsanitize=address,undefined" clobber all test check
```

If you wish to run C++ standard library containers tests, you need to set the
path to your custom versions of either gcc or libc++. For gcc run:
```
	$ make USE_CUSTOM_GCC=1 GCC_INCDIR=/path/to/includes GCC_LIBDIR=/path/to/lib check
```
If you want to use a custom version of libc++ run:
```
	$ make USE_LLVM_LIBCPP=1 LIBCPP_INCDIR=/path/to/includes/ LIBCPP_LIBDIR=/path/to/lib check
```
Please remember to set the appropriate versions of *CC/CXX* when using custom versions of the library.

For example, when using a custom version of libc++(version 3.9) installed to /usr/local/libcxx, to execute the tests run:

```
	$ CC=clang CXX=clang++ make USE_LLVM_LIBCPP=1 LIBCPP_INCDIR=/usr/local/libcxx/include/c++/v1 LIBCPP_LIBDIR=/usr/local/libcxx/lib check
```

#### Building NVML on Windows ####

Clone the NVML tree and open the solution:
```
	> git clone https://github.com/pmem/nvml
	> cd nvml/src
	> devenv NVML.sln
```

Select the desired configuration (Debug or Release) and build the solution
(i.e. by pressing Ctrl-Shift-B).


#### Testing the Libraries ####

Before running the tests, you may need to prepare a test configuration file
(src/test/testconfig.ps1).  Please see the available configuration settings
in the example file (src/test/testconfig.ps1.example).

To run the unit tests, open the PowerShell console and type:
```
	> cd nvml/src/test
	> RUNTESTS.ps1
```

To run a specific subset of tests, run for example:
```
	> RUNTESTS.ps1 -b debug -t short
```

To run just one test, run for example:
```
	> RUNTESTS.ps1 -b debug -i pmem_is_pmem
```

To modify the timeout, run:
```
	> RUNTESTS.ps1 -o 3m
```
This will set the timeout to 3 minutes.

To display all the possible options, run:
```
	> RUNTESTS.ps1 -h
```

Please refer to the **src/test/README** for more details on how to
run different types of tests.


### Experimental Packages ###

Some components in the source tree are treated as experimental. By default
those components are built but not installed (and thus not included in
packages).

If you want to build/install experimental packages run:
```
	$ make EXPERIMENTAL=y [install,rpm,dpkg]
```
**NOTE:**
The **libfabric** package required to build the **librpmem** and **rpmemd** is
not yet available on stable Debian-based distributions. This makes it
impossible to create Debian packages.

If you want to build Debian packages of **librpmem** and **rpmemd** run:
```
	$ make EXPERIMENTAL=y RPMEM_DPKG=y dpkg
```

### Contacts ###

For more information on this library, contact
Krzysztof Czurylo (krzysztof.czurylo@intel.com),
Andy Rudoff (andy.rudoff@intel.com), or post to our
[Google group](http://groups.google.com/group/pmem).
