---
title: libpmemobj
layout: nvml
---

#### The libpmemobj library

**libpmemobj** turns a persistent memory file into a
flexible object store, supporting transactions, memory
management, locking, lists, and a number of other features.

Man pages that contains a list of the interfaces provided:

<ul>
  <li>Man page for <a href="../manpages/master/{{ page.title }}.3.html">{{ page.title }} current master</a></li>
  <li>Latest releases:
    <ul>
      {% for release in site.data.releases %}{% if release.libs contains page.title %}
      <li><a href="../manpages/v{{ release.tag }}/{{ page.title }}.3.html">{{ page.title }} version {{ release.tag }}</a></li>
      {% endif %}{% endfor %}
    </ul>
  </li>
</ul>

#### libpmemobj Examples

The following series of blog articles provides a tutorial introduction
to **libpmemobj**:

* [Part 0 - new programming model](http://pmem.io/2015/06/12/pmem-model.html)
* [Part 1 - accessing the persistent memory](http://pmem.io/2015/06/13/accessing-pmem.html)
* [Part 2 - transactions](http://pmem.io/2015/06/15/transactions.html)
* [Part 3 - types](http://pmem.io/2015/06/16/types.html)
* [Part 4 - transactional dynamic memory allocation](http://pmem.io/2015/06/17/tx-alloc.html)
* [Part 5 - atomic dynamic memory allocation](http://pmem.io/2015/06/18/ntx-alloc.html)
* [Part 6 - threading](http://pmem.io/2015/06/18/threads.html)
* [Part 7 - persistent lists](http://pmem.io/2015/06/19/lists.html)
