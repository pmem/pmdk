---
title: librpmem
layout: pmdk
---

#### The librpmem library

**librpmem** provides low-level support for remote access to
*persistent memory* (pmem) utilizing RDMA-capable RNICs. The library can be
used to replicate remotely a memory region over RDMA protocol.
It utilizes appropriate persistency mechanism based on remote node's platform
capabilities. The **librpmem** utilizes the **ssh** client to authenticate
a user on remote node and for encryption of connection's out-of-band
configuration data. See **SSH** section for details.

This library is for applications that use remote persistent memory directly,
without the help of any library-supplied transactions or memory
allocation. Higher-level libraries that build on **libpmem** are
available and are recommended for most applications, see:

Man pages that contains a list of the **Linux** interfaces provided:

* Man page for <a href="../manpages/linux/master/librpmem/{{ page.title }}.7.html">{{ page.title }} current master</a>

#### The rpmemd utility

The **rpmemd** process is executed on target node by **librpmem** library over
**ssh**(1) and facilitates access to persistent memory over RDMA.

See the [rpmemd man page](../manpages/linux/master/rpmemd/rpmemd.1.html)
for current master documentation and examples or see older version:

<ul>
   {% assign command = 'rpmemd' %}
   {% for release in site.data.releases %}{% if release.libs contains command %}
   <li><a href="../manpages/linux/v{{ release.tag }}/{{ command }}.1.html">{{ command }} version {{ release.tag }}</a></li>
   {% endif %}{% endfor %}
</ul>

#### librpmem Examples

**More Detail Coming Soon**

<code data-gist-id='janekmi/47fa646ada72b27f77692672e9d7988e' data-gist-file='manpage.c' data-gist-line='36-96' data-gist-highlight-line='42' data-gist-hide-footer='true'></code>
