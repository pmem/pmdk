---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool-feature.1.html"]
title: "pmempool | PMDK"
header: "pmem Tools version 1.4"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2018-2022, Intel Corporation)

[comment]: <> (pmempool-feature.1 -- man page for pmempool-feature)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[COMPATIBILITY](#compatibility)<br />
[DISCLAIMER](#disclaimer)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool-feature** - toggle or query pool set features

# SYNOPSIS #

```
$ pmempool feature (-e|-d|-q feature-name) [options] <file>
```

# DESCRIPTION #

The **pmempool feature** command enables / disables or queries pool set features.

Available pool *feature-names* are:

+ **SINGLEHDR** - only the first part in each replica contains the pool part
internal metadata. This value can be used only with **-q**. It can not be
enabled or disabled. For details see **poolset**(5).

+ **CHECKSUM_2K** - only the first 2KiB of pool part internal metadata
is checksummed. Other features may depend on this one to store additional metadata
in otherwise unused second 2KiB part of a header.
When **CHECKSUM_2K** is disabled whole 4KiB is checksummed.

+ **SHUTDOWN_STATE** - enables additional check performed during
pool open which verifies pool consistency in the presence of dirty shutdown.
**CHECKSUM_2K** has to be enabled prior to **SHUTDOWN_STATE**
otherwise enabling **SHUTDOWN_STATE** will fail.

+ **CHECK_BAD_BLOCKS** - enables checking bad blocks performed during opening
a pool and fixing bad blocks performed by pmempool-sync during syncing a pool.
Currently (Linux kernel v4.19, libndctl v62) checking and fixing bad blocks
require read access to the following resource files (containing physical
addresses) of NVDIMM devices which only root can read by default:

```
/sys/bus/nd/devices/ndbus*/region*/resource
/sys/bus/nd/devices/ndbus*/region*/dax*/resource
/sys/bus/nd/devices/ndbus*/region*/pfn*/resource
/sys/bus/nd/devices/ndbus*/region*/namespace*/resource
```

It is possible to use poolset as *file* argument.

##### Available options: #####

`-h, --help`

Print help message.

`-v, --verbose`

Increase verbosity level.

`-e, --enable feature-name`

Enable feature for pool set.

`-d, --disable feature-name`

Disable feature for pool set.

`-q, --query feature-name`

Print feature status.

# COMPATIBILITY #

Poolsets with features not defined in this document (e.g. enabled by the newer
software version) are not supported.

# DISCLAIMER #

```pmempool feature``` command is not fail safe.

# EXAMPLE #

```
$ pmempool feature --enable CHECKSUM_2K pool.set
```

Enables POOL_FEAT_CKSUM_2K incompat feature flag.

```
$ pmempool feature --disable CHECKSUM_2K pool.set
```

Disables POOL_FEAT_CKSUM_2K incompat feature flag.

```
$ pmempool feature --query CHECKSUM_2K pool.set
0
```

Prints POOL_FEAT_CKSUM_2K incompat feature flag value.

# SEE ALSO #

**poolset**(5) and **<https://pmem.io>**
