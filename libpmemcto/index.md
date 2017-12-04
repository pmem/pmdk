---
title: libpmemcto
layout: nvml
---

#### The libpmemcto library

**libpmemcto** is a persistent memory allocator with no overhead imposed
by run-time flushing or transactional updates.  An overhead is imposed only
when program exits normally and have to flush the pool contents.
If the program crashes before flushing the file (or if flushing fails),
the pool is in an inconsistent state causing subsequent pool opening to fail.
**libpmemcto** provides common malloc-like interfaces to persistent memory
pools built on memory-mapped files.

Man pages that contains a list of the **Linux** interfaces provided:

* Man page for <a href="../manpages/linux/master/libpmemcto/{{ page.title }}.7.html">{{ page.title }} current master</a>


Man pages that contains a list of the **Windows** interfaces provided:

* Man page for <a href="../manpages/windows/master/libpmemcto/{{ page.title }}.7.html">{{ page.title }} current master</a>

#### libpmemcto Examples

**More Detail Coming Soon**

<code data-gist-id='krzycz/f2f3b95eb5f84d6a77513aef9c3e9b70' data-gist-file='manpage.c' data-gist-line='37-97' data-gist-highlight-line='45' data-gist-hide-footer='true'></code>
