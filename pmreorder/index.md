---
title: pmreorder
layout: pmdk
---

#### The pmreorder utility

**pmreorder** is a standalone python tool that performs
consistency check of persistent programs using a store reordering mechanism.

See the [pmreorder man page](../manpages/linux/master/pmreorder/pmreorder.1.html)
for current master documentation and examples or see older versions:

<ul>
   {% assign command = 'pmreorder' %}
   {% for release in site.data.releases_linux %}{% if release.libs contains command and release.tag != "master" %}
   <li><a href="../manpages/linux/v{{ release.tag }}/pmreorder/{{ command }}.1.html">{{ command }} version {{ release.tag }}</a></li>
   {% endif %}{% endfor %}
</ul>
