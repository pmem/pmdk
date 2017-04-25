---
title: libpmemlog
layout: nvml
---

#### The libpmemlog library

**libpmemlog** implements a pmem-resident log file.

This library is provided for cases requiring an append-mostly
file to record variable length entries.  Most
developers will find higher level libraries like
[libpmemobj](../libpmemobj) to be more generally useful.

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

#### libpmemlog Examples

**More Detail Coming Soon**

<code data-gist-id='andyrudoff/9f0aee99c7b046fdb974' data-gist-file='manpage.c' data-gist-line='37-98' data-gist-highlight-line='43' data-gist-hide-footer='true'></code>
