---
title: libpmempool
layout: pmdk
---

#### The libpmempool library

**libpmempool** provides a set of utilities for management, diagnostics and
repair of persistent memory pools.
By pool in this context we mean pmemobj pool, pmemblk pool, pmemlog pool or
BTT layout, independent of the underlying storage.
The **libpmempool** is for applications that need high reliability or built-in
troubleshooting. It may be useful for testing and debugging purposes also.

Man pages that contains a list of the **Linux** interfaces provided:

* Man page for <a href="../manpages/linux/master/libpmempool/{{ page.title }}.7.html">{{ page.title }} current master</a>


Man pages that contains a list of the **Windows** interfaces provided:

* Man page for <a href="../manpages/windows/master/libpmempool/{{ page.title }}.7.html">{{ page.title }} current master</a>

#### libpmempool Examples

**More Detail Coming Soon**

<code data-gist-id='krzycz/53f5b5f33cc6bfbbd80c04a3209202d0' data-gist-file='manpage.c' data-gist-line='37-96' data-gist-highlight-line='41' data-gist-hide-footer='true'></code>

##### libpmempool transform #####

**Example 1.**

Let files `/path/poolset_file_src` and `/path/poolset_file_dst` have the
following contents:


<code data-gist-id='wojtuss/06c22e3a8340e85574cc89d767ae2534' data-gist-file='poolset_file_src' data-gist-hide-footer='true'></code>

<code data-gist-id='wojtuss/b9693dd0f1f8962bf01eb0791a68128a' data-gist-file='poolset_file_dst' data-gist-hide-footer='true'></code>

Then

`pmempool_transform("/path/poolset_file_src", "/path/poolset_file_dst", 0);`

adds a replica to the poolset. All other replicas remain unchanged and
the size of the pool remains 60M.


**Example 2.**

Let files `/path/poolset_file_src` and `/path/poolset_file_dst` have the
following contents:

<code data-gist-id='wojtuss/06c22e3a8340e85574cc89d767ae2534' data-gist-file='poolset_file_src' data-gist-hide-footer='true'></code>

<code data-gist-id='wojtuss/f535a8ced7a34522f8b6189c9ddd7e89' data-gist-file='poolset_file_dst' data-gist-hide-footer='true'></code>

Then

`pmempool_transform("/path/poolset_file_src", "/path/poolset_file_dst", 0);`

deletes the second replica from the poolset. The first replica remains unchanged and
the size of the pool is still 60M.

