---
layout: manual
Content-Style: 'text/css'
title: PMEMPOOL_FEATURE_QUERY
collection: libpmempool
header: PMDK
date: pmempool API version 1.3
...

[comment]: <> (Copyright 2018, Intel Corporation)

[comment]: <> (Redistribution and use in source and binary forms, with or without)
[comment]: <> (modification, are permitted provided that the following conditions)
[comment]: <> (are met:)
[comment]: <> (    * Redistributions of source code must retain the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer.)
[comment]: <> (    * Redistributions in binary form must reproduce the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer in)
[comment]: <> (      the documentation and/or other materials provided with the)
[comment]: <> (      distribution.)
[comment]: <> (    * Neither the name of the copyright holder nor the names of its)
[comment]: <> (      contributors may be used to endorse or promote products derived)
[comment]: <> (      from this software without specific prior written permission.)

[comment]: <> (THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS)
[comment]: <> ("AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR)
[comment]: <> (A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT)
[comment]: <> (OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,)
[comment]: <> (SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,)
[comment]: <> (DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY)
[comment]: <> (THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT)
[comment]: <> ((INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE)
[comment]: <> (OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)

[comment]: <> (pmempool_feature_query.3 -- man page for toggle and query pool
set features)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[COMPATIBILITY](#compatibility)<br />
[DISCLAIMER](#disclaimer)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmempool_feature_query**(), **pmempool_feature_enable**(),
**pmempool_feature_disable**() - toggle or query pool set features


# SYNOPSIS #

```c
#include <libpmempool.h>

int pmempool_feature_query(const char *path, enum pmempool_feature feature, unsigned flags);

int pmempool_feature_enable(const char *path, enum pmempool_feature feature, unsigned flags);

int pmempool_feature_disable(const char *path, enum pmempool_feature feature, unsigned flags);
```




# DESCRIPTION #

The *feature* argument accepts following values:

+ **PMEMPOOL_FEAT_SINGLEHDR** - only the first part in each replica contains the
pool part internal metadata. This value can be used only with
**pmempool_feature_query**(). It can not be enabled or disabled. For details see
**poolset**(5).

+ **PMEMPOOL_FEAT_CKSUM_2K** - only the first 2KiB of pool part internal metadata
is checksummed. Other features may depend on this one to store additional metadata
in otherwise unused second 2KiB part of a header.
When **PMEMPOOL_FEAT_CKSUM_2K** is disabled whole 4KiB is checksummed.

+ **PMEMPOOL_FEAT_SHUTDOWN_STATE** - enables additional check performed during
pool open which verifies pool consistency in the presence of dirty shutdown.
**PMEMPOOL_FEAT_CKSUM_2K** has to be enabled prior to
**PMEMPOOL_FEAT_SHUTDOWN_STATE** otherwise enabling **PMEMPOOL_FEAT_SHUTDOWN_STATE** will fail.

+ **PMEMPOOL_FEAT_CHECK_BAD_BLOCKS** - enables checking bad blocks performed
during opening a pool and fixing bad blocks performed by pmempool-sync
during syncing a pool. For details see **pmempool-feature**(1).

The **pmempool_feature_query**() function checks state of *feature* in the
pool set pointed by *path*.

The **pmempool_feature_enable**() function enables *feature* in the pool set
pointed by *path*.

The **pmempool_feature_disable**() function disables *feature* in the pool set
pointed by *path*.

# COMPATIBILITY #

Poolsets with features not defined in this document (e.g. enabled by the newer
software version) are not supported.

# DISCLAIMER #

**pmempool_feature_query**(), **pmempool_feature_enable**() and
**pmempool_feature_disable**() are not fail safe.

# RETURN VALUE #

On success, **pmempool_feature_query**() returns 0 if *feature* is disabled or
1 if it is enabled. On error, it returns -1 and sets *errno* accordingly.

On success, **pmempool_feature_enable**() returns 0. On error, it returns -1
and sets *errno* accordingly.

On success, **pmempool_feature_disable**() returns 0. On error, it returns -1
and sets *errno* accordingly.

If *path* points poolset with remote replica **errno** is set to EINVAL and
function returns -1.

If non zero *flags* are provided **errno** is set to EINVAL and function
returns -1.

# SEE ALSO #

**poolset**(5) and **<http://pmem.io>**
