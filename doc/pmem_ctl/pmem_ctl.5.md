---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem_ctl.5.html"]
title: "pmem_ctl | PMDK"
header: "pmem_ctl API version 1.4"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2018-2019, Intel Corporation)

[comment]: <> (pmem_ctl.5 -- man page for CTL)

[NAME](#name)<br />
[DESCRIPTION](#description)<br />
[CTL EXTERNAL CONFIGURATION](#ctl-external-configuration)<br />
[SEE ALSO](#see-also)<br />

# NAME #

ctl - interface for examination and modification of the library's internal state.

# DESCRIPTION #

The CTL namespace is organized in a tree structure. Starting from the root,
each node can be either internal, containing other elements, or a leaf.
Internal nodes themselves can only contain other nodes and cannot be entry
points. There are two types of those nodes: *named* and *indexed*. Named nodes
have string identifiers. Indexed nodes represent an abstract array index and
have an associated string identifier. The index itself is provided by the user.
A collection of indexes present on the path of an entry point is provided to
the handler functions as name and index pairs.

Entry points are the leaves of the CTL namespace structure. Each entry point
can read from the internal state, write to the internal state,
exec a function or a combination of these operations.

The entry points are listed in the following format:

name | r(ead)w(rite)x(ecute) | global/- | read argument type | write argument type | exec argument type | config argument type

A description of **pmem_ctl** functions can be found on the following
manual pages:
**libpmemblk_ctl_get**(3), **libpmemlog_ctl_get**(3), **libpmemobj_ctl_get**(3)

# CTL EXTERNAL CONFIGURATION #

In addition to direct function call, each write entry point can also be set
using two alternative methods.

The first method is to load a configuration directly from the
**PMEMBLK_CONF**/ **PMEMLOG_CONF**/ **PMEMOBJ_CONF** environment variable.
A properly formatted ctl config string is a single-line
sequence of queries separated by ';':

```
query0;query1;...;queryN
```

A single query is constructed from the name of the ctl write entry point and
the argument, separated by '=':

```
entry_point=entry_point_argument
```

The entry point argument type is defined by the entry point itself, but there
are three predefined primitives:

	*) integer: represented by a sequence of [0-9] characters that form
		a single number.
	*) boolean: represented by a single character: y/n/Y/N/0/1, each
		corresponds to true or false. If the argument contains any
		trailing characters, they are ignored.
	*) string: a simple sequence of characters.

There are also complex argument types that are formed from the primitives
separated by a ',':

```
first_arg,second_arg
```

In summary, a full configuration sequence looks like this:

```
(first_entry_point)=(arguments, ...);...;(last_entry_point)=(arguments, ...);
```

As an example, to set both prefault at_open and at_create variables:
```

PMEMBLK_CONF="prefault.at_open=1;prefault.at_create=1"
```

The second method of loading an external configuration is to set the
**PMEMBLK_CONF_FILE**/ **PMEMLOG_CONF_FILE**/ **PMEMOBJ_CONF_FILE**
environment variable to point to a file that contains
a sequence of ctl queries. The parsing rules are all the same, but the file
can also contain white-spaces and comments.

To create a comment, simply use '#' anywhere in a line and everything
afterwards, until a new line, will be ignored.

An example configuration file:

```
#########################
# My pmemblk configuration
#########################
#
# Global settings:
prefault. # modify the behavior of pre-faulting
	at_open = 1; # prefault when the pool is opened

prefault.
	at_create = 0; # but don't prefault when it's created

# Per-pool settings:
# ...

```
# SEE ALSO #

**libpmemblk_ctl_get**(3), **libpmemlog_ctl_get**(3), **libpmemobj_ctl_get**(3)
and **<https://pmem.io>**
