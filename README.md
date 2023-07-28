# **PMDK: Persistent Memory Development Kit**

[![GHA build status](https://github.com/pmem/pmdk/workflows/PMDK/badge.svg?branch=master)](https://github.com/pmem/pmdk/actions)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/3015.svg)](https://scan.coverity.com/projects/pmem-pmdk)
[![Coverage Status](https://codecov.io/github/pmem/pmdk/coverage.svg?branch=master)](https://codecov.io/gh/pmem/pmdk/branch/master)
[![PMDK release version](https://img.shields.io/github/release/pmem/pmdk.svg?sort=semver)](https://github.com/pmem/pmdk/releases/latest)
[![Packaging status](https://repology.org/badge/tiny-repos/pmdk.svg)](https://repology.org/project/pmdk/versions)
[![CodeQL status](https://github.com/pmem/pmemstream/actions/workflows/codeql.yml/badge.svg?branch=master)](https://github.com/pmem/pmemstream/actions/workflows/codeql.yml)
[![Security: bandit](https://img.shields.io/badge/security-bandit-yellow.svg?branch=master)](https://github.com/pmem/pmdk/actions/workflows/bandit.yml)

The **Persistent Memory Development Kit (PMDK)** is a collection of libraries and tools for System Administrators and Application Developers to simplify managing and accessing persistent memory devices. For more information, see https://pmem.io.

To install PMDK libraries, either install pre-built packages, which we build for every stable release, or clone the tree and build it yourself. **Pre-built** packages can be found in popular Linux distribution package repositories, or you can check out our recent stable releases on our [github release page](https://github.com/pmem/pmdk/releases). Specific installation instructions are outlined below.

Bugs and feature requests for this repo are tracked in our [GitHub Issues Database](https://github.com/pmem/pmdk/issues).

## Contents
1. [Libraries and Utilities](#libraries-and-utilities)
2. [Getting Started](#getting-started)
3. [Version Conventions](#version-conventions)
4. [Building and installing](#building-and-installing)
5. [Experimental Supports](#experimental-supports)
	* [Experimental Support for 64-bit ARM](#experimental-support-for-64-bit-arm-and-risc-v)
	* [Experimental Support for PowerPC](#experimental-support-for-powerpc)
6. [Archived and deprecated libraries](#archived-and-deprecated-libraries)
7. [Contact Us](#contact-us)

## Libraries and Utilities

All PMDK related libraries are described in detail on [pmem.io/pmdk](https://pmem.io/pmdk/).

Libraries available in this repository:

- [libpmem](https://pmem.io/pmdk/libpmem/): provides low-level persistent memory support.
- [libpmem2](https://pmem.io/pmdk/libpmem2/): provides low-level persistent memory support, is a new version of libpmem.
- [libpmemobj](https://pmem.io/pmdk/libpmemobj/): provides a transactional object store, providing memory allocation, transactions, and general facilities for persistent memory programming.
- [libpmempool](https://pmem.io/pmdk/libpmempool/): provides support for off-line pool management and diagnostics.

Utilities available in this repository:

- [pmempool](https://pmem.io/pmdk/pmempool/): allows managing and analyzing persistent memory pools.
- [pmemcheck](https://pmem.io/2015/07/17/pmemcheck-basic.html): a Valgrind tool for persistent memory error detection.

Currently, these libraries and utilities only work on 64-bit Linux.

See our [LICENSE](LICENSE) file for information on how these libraries are licensed.

## Getting Started

Getting Started with Persistent Memory Programming is a tutorial series created by Intel architect, Andy Rudoff. In this tutorial, you will be introduced to persistent memory programming and learn how to apply it to your applications.
- Part 1: [What is Persistent Memory?](https://software.intel.com/en-us/persistent-memory/get-started/series)
- Part 2: [Describing The SNIA Programming Model](https://software.intel.com/en-us/videos/the-nvm-programming-model-persistent-memory-programming-series)
- Part 3: [Introduction to PMDK Libraries](https://software.intel.com/en-us/videos/intro-to-the-nvm-libraries-persistent-memory-programming-series)
- Part 4: [Thinking Transactionally](https://software.intel.com/en-us/videos/thinking-transactionally-persistent-memory-programming-series)
- Part 5: [A C++ Example](https://software.intel.com/en-us/videos/a-c-example-persistent-memory-programming-series)

Additionally, we recommend reading [Introduction to Programming with Persistent Memory from Intel](https://software.intel.com/en-us/articles/introduction-to-programming-with-persistent-memory-from-intel)

## Version Conventions

- **Builds** are tagged something like `0.2+b1`, which means _Build 1 on top of version 0.2_
- **Release Candidates** have a '-rc{version}' tag, e.g. `0.2-rc3`, meaning _Release Candidate 3 for version 0.2_
- **Stable Releases** use a _major.minor_ tag like `0.2`

## Building and installing

Install a few [dependencies](INSTALL.md#dependencies) and then build and install PMDK in the system.

```sh
# get the source code
git clone https://github.com/pmem/pmdk
cd pmdk
# build
make -j
# install (optionally)
sudo make install
```

If experience any issues or looking for additional options, check out the [INSTALL.md](INSTALL.md) file or [contact us](#contact-us).

## Experimental Supports

### Experimental Support for 64-bit ARM and RISC-V

There is initial support for 64-bit ARM and RISC-V processors provided.
It is currently not validated nor maintained.
Thus, these architectures should not be used in a production environment.

### Experimental Support for PowerPC

There is initial support for ppc64le processors provided.
It is currently not validated nor maintained.
Thus, this architecture should not be used in a production environment.

The on-media pool layout is tightly attached to the page size
of 64KiB used by default on ppc64le, so it is not interchangeable with
different page sizes, includes those on other architectures. For more
information on this port, contact Rajalakshmi Srinivasaraghavan
(rajis@linux.ibm.com) or Lucas Magalhães (lucmaga@gmail.com).

## Archived and deprecated libraries

- [libpmemblk](https://pmem.io/pmdk/libpmemblk/): supports arrays of pmem-resident blocks, all the same size, that are atomically updated. The final release was [1.13.1](https://github.com/pmem/pmdk/releases/tag/1.13.1).
- [libpmemlog](https://pmem.io/pmdk/libpmemlog/): provides a pmem-resident log file. The final release was [1.13.1](https://github.com/pmem/pmdk/releases/tag/1.13.1).
- [libpmemset](https://pmem.io/pmdk/libpmemset/): provides support for persistent file I/O operations, runtime mapping concatenation and multi-part support across poolsets. The final release was [1.12.1](https://github.com/pmem/pmdk/releases/tag/1.12.1).
- [librpmem](https://pmem.io/pmdk/librpmem/): provides low-level support for remote access to persistent memory utilizing RDMA-capable RNICs. The final release was [1.12.1](https://github.com/pmem/pmdk/releases/tag/1.12.1). If you are interested in remote persistent memory support you might be also interested in the [librpma](https://github.com/pmem/rpma) library.
- [libvmem](https://pmem.io/vmem/libvmem/): turns a pool of persistent memory into a volatile memory pool, similar to the system heap but kept separate and with its own malloc-style API. It has been moved to a [separate repository](https://github.com/pmem/vmem).
- [libvmemalloc](https://pmem.io/vmem/libvmmalloc/): transparently converts all the dynamic memory allocations into persistent memory allocations. This allows the use of persistent memory as volatile memory without modifying the target application. It has been moved to a [separate repository](https://github.com/pmem/vmem).

## Contact Us

For more information on this library, contact
Piotr Balcer (piotr.balcer@intel.com),
Andy Rudoff (andy.rudoff@intel.com), or post to our
[Google group](https://groups.google.com/group/pmem).
