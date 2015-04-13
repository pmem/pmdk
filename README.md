nvml: Linux NVM Library
=======================

[![Build Status](https://travis-ci.org/pmem/nvml.svg)](https://travis-ci.org/pmem/nvml)

This is the top-level README.md the Linux NVM Library.
For more information, see http://pmem.io.

Please see the file LICENSE for information on how this library is licensed.

This tree contains libraries for using Non-Volatile Memory (NVM).
Here you'll find:

* **doc** -- man pages describing each library contained here
* **src** -- the source for the libraries
* **src/benchmarks** -- benchmarks used by development team
* **src/examples** -- brief example programs using these libraries
* **src/test** -- unit tests used by development team
* **src/tools** -- various tools developed for NVML
* **utils** -- utilities used during build & test
* **CONTRIBUTING.md** -- instructions for people wishing to contribute

To build this library, you may need to install some required packages on
the build system.  See the **before_install:** rules in the
[.travis.yml](https://github.com/pmem/nvml/blob/master/.travis.yml)
file at the top level of the repository to get an idea what packages
were required to build on the _travis-ci_ (Ubuntu-based) systems.

Once the build system is setup, the NVM Library is built using
this command at the top level:
```
	$ make
```

To build and run the unit tests:
```
	$ make check
```

To compile this library with enabled support for the PM-aware version
of [Vaglrind](https://github.com/pmem/valgrind), supply the compiler
with the **USE_VALGRIND** flag, for example:
```
	$ make EXTRA_CFLAGS=-DUSE_VALGRIND
```

To install this library into the standard locations
(/usr/lib, /usr/include, /usr/share/man), become root and:
```
	$ make install
```

To install this library into other locations, you can use
DESTDIR variable, e.g.:
```
	$ make install DESTDIR=/tmp
```
This will install files to /tmp/lib, /tmp/include /tmp/usr/share/man.

To install a complete copy of the source tree to $(DESTDIR)/nvml:
```
	$ make source DESTDIR=some_path
```

To build rpm packages on rpm-based distributions:
```
	$ make rpm
```
**Prerequisites:** rpmbuild

To build dpkg packages on Debian-based distributions:
```
	$ make dpkg
```
**Prerequisites:** devscripts


For more information on this library,
contact Andy Rudoff (andy.rudoff@intel.com).
