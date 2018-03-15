---
title: daxio
layout: pmdk
---

#### The daxio utility

**daxio** is a standalone utility that may be used to transfer data to or from
Device DAX device or to wipe out data from a device.

See the [daxio man page](../manpages/linux/master/daxio/daxio.1.html)
for current master documentation and examples or see older versions:

<ul>
   {% assign command = 'daxio' %}
   {% for release in site.data.releases_linux %}{% if release.libs contains command and release.tag != "master" %}
   <li><a href="../manpages/linux/v{{ release.tag }}/daxio/{{ command }}.1.html">{{ command }} version {{ release.tag }}</a></li>
   {% endif %}{% endfor %}
</ul>
