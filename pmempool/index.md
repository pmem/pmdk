---
title: pmempool
layout: pmdk
---

#### The pmempool utility

**pmempool** is a standalone utility for management and off-line analysis
of persistent memory pools. It works for both the *single-file* pools and
for *pool set files*. Below you can find a list of available commands.

See the [pmempool man page](../manpages/linux/master/pmempool/pmempool.1.html)
for current master documentation and examples or see older versions:

<ul>
   {% assign command = 'pmempool' %}
   {% for release in site.data.releases_linux %}{% if release.libs contains command and release.tag != "master" %}
   <li><a href="../manpages/linux/v{{ release.tag }}/pmempool/{{ command }}.1.html">{{ command }} version {{ release.tag }}</a></li>
   {% endif %}{% endfor %}
</ul>

##### create

The **pmempool create** command creates a persistent memory pool of specified
type and parameters. Creating the persistent memory pool is possible using
the NVM library APIs, however using the **pmempool** utility for creating the
pool files may simplify the user application.

See the [pmempool create man page](../manpages/linux/master/pmempool/pmempool-create.1.html)
for current documentation and examples or see older versions:

<ul>
   {% assign command = 'pmempool-create' %}
   {% for release in site.data.releases_linux %}{% if release.libs contains command and release.tag != "master" %}
   <li><a href="../manpages/linux/v{{ release.tag }}/pmempool/{{ command }}.1.html">{{ command }} version {{ release.tag }}</a></li>
   {% endif %}{% endfor %}
</ul>

##### info

The **pmempool info** command performs an off-line analysis of a persistent memory
pool. The **pmempool info** command prints all persistent data
structures and evaluates some basic statistics of the pool. By default the
**pmempool info** prints the *pool header* and *pool descriptor* data
structures, but by adding command line arguments it is possible to print
additional information about the pool.

See the [pmempool info man page](../manpages/linux/master/pmempool/pmempool-info.1.html)
for current master documentation and examples or see older versions:

<ul>
   {% assign command = 'pmempool-info' %}
   {% for release in site.data.releases_linux %}{% if release.libs contains command and release.tag != "master" %}
   <li><a href="../manpages/linux/v{{ release.tag }}/pmempool/{{ command }}.1.html">{{ command }} version {{ release.tag }}</a></li>
   {% endif %}{% endfor %}
</ul>

##### dump

The **pmempool dump** command dumps the user data stored in a persistent memory
pool file. This command currently works only for the **pmemblk** and
**pmemlog** pool types. It is possible to pick the data dump format as well as
the range of desired data.

See the [pmempool dump man page](../manpages/linux/master/pmempool/pmempool-dump.1.html)
for current master documentation and examples or see older versions:

<ul>
   {% assign command = 'pmempool-dump' %}
   {% for release in site.data.releases_linux %}{% if release.libs contains command and release.tag != "master" %}
   <li><a href="../manpages/linux/v{{ release.tag }}/pmempool/{{ command }}.1.html">{{ command }} version {{ release.tag }}</a></li>
   {% endif %}{% endfor %}
</ul>

##### check

The **pmempool check** command checks consistency of the persistent memory pool.
It prints information about errors found. This command is able to repair
a broken pool in some cases. Currently it works only for the **pmemblk** and
**pmemlog** pool types but the support for **pmemobj** pool type will be
available in the near feature.

See the [pmempool check man page](../manpages/linux/master/pmempool/pmempool-check.1.html)
for current master documentation and examples or see older version:

<ul>
   {% assign command = 'pmempool-check' %}
   {% for release in site.data.releases_linux %}{% if release.libs contains command and release.tag != "master" %}
   <li><a href="../manpages/linux/v{{ release.tag }}/pmempool/{{ command }}.1.html">{{ command }} version {{ release.tag }}</a></li>
   {% endif %}{% endfor %}
</ul>

##### rm

The **pmempool rm** is a simple replacement for the system **rm** command for
the persistent memory pools. The command may be useful for removing the
*pool set files* because it removes all part files specified in the
configuration file. For *single-file* pools it works almost the same
as the standard system **rm** command.

See the [pmempool rm man page](../manpages/linux/master/pmempool/pmempool-rm.1.html)
for current master documentation and examples or see older version:

<ul>
   {% assign command = 'pmempool-rm' %}
   {% for release in site.data.releases_linux %}{% if release.libs contains command and release.tag != "master" %}
   <li><a href="../manpages/linux/v{{ release.tag }}/pmempool/{{ command }}.1.html">{{ command }} version {{ release.tag }}</a></li>
   {% endif %}{% endfor %}
</ul>

##### convert

The **pmempool convert** command performs conversion of the specified pool
from the old layout versions to the newest one supported by this tool.
Currently only **pmemobj** pools are supported.

Starting from PMDK 1.5 **pmempool convert** is a thin wrapper around a tool
called **pmdk-convert**. This tool handles all versions of PMDK pools and is
the recommended conversion mechanism.

See the [pmdk-convert man page](https://pmem.io/pmdk-convert/manpages/master/pmdk-convert.1.html) for the current master documentation or see the older version:
<ul>
<li> <a href="https://pmem.io/pmdk-convert/manpages/v1.5/pmdk-convert.1.html">pmdk-convert version 1.5</a></li>
</ul>

For documentation of **pmempool convert** for older PMDK versions see:
<ul>
   {% assign command = 'pmempool-convert' %}
   {% for release in site.data.releases_linux %}{% if release.libs contains command and release.tag != "master" %}
   <li><a href="../manpages/linux/v{{ release.tag }}/pmempool/{{ command }}.1.html">{{ command }} version {{ release.tag }}</a></li>
   {% endif %}{% endfor %}
</ul>

##### sync

The **pmempool sync** command synchronizes data between replicas within
a poolset. It checks if metadata of all replicas in a poolset
are consistent, i.e. all parts are healthy, and if any of them is not,
the corrupted or missing parts are recreated and filled with data from one of
the healthy replicas.

See the [pmempool sync man page](../manpages/linux/master/pmempool/pmempool-sync.1.html)
for current master documentation and examples or see older version:

<ul>
   {% assign command = 'pmempool-sync' %}
   {% for release in site.data.releases_linux %}{% if release.libs contains command and release.tag != "master" %}
   <li><a href="../manpages/linux/v{{ release.tag }}/pmempool/{{ command }}.1.html">{{ command }} version {{ release.tag }}</a></li>
   {% endif %}{% endfor %}
</ul>

##### transform

The **pmempool transform** command modifies internal structure of a poolset
defined by one poolset file, according to a structure described in
another poolset file.

See the [pmempool transform man page](../manpages/linux/master/pmempool/pmempool-transform.1.html)
for current master documentation and examples or see older version:

<ul>
   {% assign command = 'pmempool-transform' %}
   {% for release in site.data.releases_linux %}{% if release.libs contains command and release.tag != "master" %}
   <li><a href="../manpages/linux/v{{ release.tag }}/pmempool/{{ command }}.1.html">{{ command }} version {{ release.tag }}</a></li>
   {% endif %}{% endfor %}
</ul>

##### feature

The **pmempool feature** command enables / disables or query pool set features.

See the [pmempool feature man page](../manpages/linux/master/pmempool/pmempool-feature.1.html)
for current master documentation and examples.
