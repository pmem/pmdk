---
title: libvmem
layout: nvml
---

#### The libvmem library

**libvmem** supports the traditional _malloc_/_free_
interfaces on a memory mapped file.  This allows the
use of persistent memory as volatile memory, for cases
where the pool of persistent memory is useful to an
application, but when the application doesn't need
it to be persistent.

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

#### libvmem Examples

**More Detail Coming Soon**

<code data-gist-id='andyrudoff/02a10ca6b9ab7d07922b' data-gist-file='manpage.c' data-gist-line='37-66' data-gist-highlight-line='40' data-gist-hide-footer='true'></code>
