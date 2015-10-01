---
title: NVM Library
layout: nvml
---

The NVM Library is a library for using memory-mapped persistence,
optimized specifically for _persistent memory_.  The source is in this
[GitHub repository](https://github.com/pmem/nvml/).  The latest
release (source and pre-built packages for some common Linux distros)
can be found in the
[releases area on GitHub](https://github.com/pmem/nvml/releases).

**Note: The NVM Library is still under development and has been validated
to "alpha" quality, meaning it is fully functional with no known
critical bugs, but performance and reliability work is still ongoing.**

The NVM Library is actually six separate libraries:

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

#### libvmem

The **libvmem** library turns a pool of persistent memory into a
volatile memory pool, similar to the system heap but kept separate
and with its own malloc-style API.

See the [libvmem page](libvmem) for documentation and examples.

#### libvmmalloc

The **libvmmalloc** library **transparently** converts all the dynamic
memory allocations into persistent memory allocations.  This allows the use
of persistent memory as volatile memory without modifying the target
application.

See the [libvmmalloc page](libvmmalloc) for documentation and examples.

#### pmempool

The **pmempool** is a management tool for Persistent Memory pool files created
by the **NVML** libraries. It may be useful for system administrators as well
as for software developers for troubleshooting and debugging purposes.

See the [pmempool page](pmempool) for available commands and documentation.
