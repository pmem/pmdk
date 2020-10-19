---
title: PMDK
layout: pmdk
---
The Persistent Memory Development Kit (PMDK),
formerly known as [NVML](../2017/12/11/NVML-is-now-PMDK.html),
is a growing collection of libraries and tools.
Tuned and validated on both Linux and Windows, the libraries build on
the DAX feature of those operating systems (short for _Direct Access_)
which allows applications to access persistent memory as _memory-mapped files_,
as described in the
[SNIA NVM Programming Model](https://www.snia.org/sites/default/files/technical_work/final/NVMProgrammingModel_v1.2.pdf).

The source for PMDK is in this
[GitHub repository](https://github.com/pmem/pmdk/).

The following libraries are part of PMDK:

#### libpmemobj

The **libpmemobj** library provides a transactional object store,
providing memory allocation, transactions, and general facilities
for persistent memory programming.  Developers new to persistent
memory probably want to start with this library.

See the [libpmemobj page](libpmemobj) for documentation and examples.

#### libpmemblk

The **libpmemblk** library supports arrays of pmem-resident blocks,
all the same size, that are atomically updated.  For example, a
program keeping a cache of fixed-size objects in pmem might find
this library useful.

See the [libpmemblk page](libpmemblk) for documentation and examples.

#### libpmemlog

The **libpmemlog** library provides a pmem-resident log file.
This is useful for programs like databases that append frequently
to a log file.

See the [libpmemlog page](libpmemlog) for documentation and examples.

#### libpmem

The **libpmem** library provides low level persistent memory support.
The libraries above are implemented using **libpmem**.  Developers
wishing to _roll their own_ persistent memory algorithms will find
this library useful, but most developers will likely use **libpmemobj**
above and let that library call **libpmem** for them.

See the [libpmem page](libpmem) for documentation and examples.

#### libpmem2

The **libpmem2** library provides low level persistent memory support.
The library is a new version of **libpmem**. **libpmem2** provides a
more universal and platform-agnostic interface. Developers
wishing to _roll their own_ persistent memory algorithms will find
this library useful, but most developers will likely use **libpmemobj**
above that provides memory allocation and transactions support.

See the [libpmem2 page](libpmem2) for documentation and examples.

#### libvmem

The **libvmem** library turns a pool of persistent memory into a
volatile memory pool, similar to the system heap but kept separate
and with its own malloc-style API.

See the [libvmem page](../vmem/libvmem/) for documentation and examples.

>NOTE:
Since persistent memory support
has been integrated into [libmemkind](https://github.com/memkind/memkind),
that library is the **recommended** choice for any new volatile usages,
since it combines support for multiple types of volatile memory into
a single, convenient API.

#### libvmmalloc

The **libvmmalloc** library **transparently** converts all the dynamic
memory allocations into persistent memory allocations.  This allows the use
of persistent memory as volatile memory without modifying the target
application.

See the [libvmmalloc page](../vmem/libvmmalloc/) for documentation and examples.

#### libpmempool

The **libpmempool** provides support for off-line pool management and
diagnostics.  Currently it provides only "check" and "repair" operations
for **pmemlog** and **pmemblk** memory pools, and for BTT devices.

See the [libpmempool page](libpmempool) for documentation and examples.

#### pmempool

**pmempool** is a management tool for persistent memory pool files created
by the PMDK libraries. It may be useful for system administrators as well
as for software developers for troubleshooting and debugging.

See the [pmempool page](pmempool) for available commands and documentation.

#### librpmem

The **librpmem** provides low-level support for remote access to
_persistent memory_ utilizing RDMA-capable RNICs. The library can be
used to replicate content of local persistent memory regions to
persistent memory on a remote node over RDMA protocol.

See the [librpmem page](librpmem) for documentation and examples.

>NOTE:
This is still an **experimental API** and should not be used in production
environments.

>NOTE:
The alternative solution for accessing remote persistent memory is implemented
by the [librpma](index.md#librpma) library (see below).

#### librpma

**librpma** is a C library to simplify accessing persistent memory
on remote hosts over Remote Direct Memory Access (RDMA).

See the [librpma page](../rpma/) for available documentation.

#### libpmemset

**libpmemset** aims to provide support for persistent file I/O operations,
runtime mapping concatenation and multi-part support across poolsets.
It relies on synchronous event streams for pool modifications.

>NOTE:
This is still an **experimental API** and should not be used in production
environments.

See the [libpmemset page](../libpmemset/) for updates.

#### libvmemcache

**libvmemcache** is an embeddable and lightweight in-memory caching solution.
It's designed to fully take advantage of large capacity memory, such as
persistent memory with DAX, through memory mapping in an efficient
and scalable way.

See the [libvmemcache](../vmemcache/manpages/master/vmemcache.3.html)
for current documentation.

#### daxio

The **daxio** is a utility that performs I/O on Device DAX devices or zero
a Device DAX device.

See the [daxio page](daxio) for available commands and documentation.

#### pmreorder

The **pmreorder** is an utility that performs a consistency check of a persistent program.

See the [pmreorder page](pmreorder) for available commands and documentation.

#### pmdk-convert

The **pmdk-convert** tool performs conversion of the specified pool
from the old layout versions to the newest one supported by this tool.

See the [pmdk-convert](../pmdk-convert/manpages/master/pmdk-convert.1.html)
for current documentation.

#### C++ bindings

The C++ bindings aim at providing an easier to use, less error prone
implementation of **libpmemobj**. The C++ implementation requires a compiler
compliant with C++11 and one feature requires C++17.

See the [C++ bindings page](../libpmemobj-cpp/) for documentation and examples.

#### pmemkv

**pmemkv** is a local/embedded key-value datastore optimized for persistent memory.
Rather than being tied to a single language or backing implementation,
**pmemkv** provides different options for language bindings and storage engines.

See the [pmemkv page](../pmemkv/) for available documentation.
