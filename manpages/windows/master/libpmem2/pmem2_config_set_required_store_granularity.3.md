---
layout: manual
Content-Style: 'text/css'
title: PMEM2_CONFIG_SET_REQUIRED_STORE_GRANULARITY
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (Copyright 2019, Intel Corporation)

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

[comment]: <> (pmem2_config_set_required_store_granularity.3 -- man page for pmem2_config_set_required_store_granularity

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_required_store_granularity**() - set a granularity
in pmem2_config structure.

# SYNOPSIS #

```c
#include <libpmem2.h>

enum pmem2_granularity {
	PMEM2_GRANULARITY_BYTE,
	PMEM2_GRANULARITY_CACHE_LINE,
	PMEM2_GRANULARITY_PAGE,
};
int pmem2_config_set_required_store_granularity(struct pmem2_config *cfg,
		enum pmem2_granularity g);
```

# DESCRIPTION #

The **pmem2_config_set_required_store_granularity**() sets a maximum permitted
granularity *g* requested by user in the *pmem2_config* structure.

Granularity must be one of the following values:

 * **PMEM2_GRANULARITY_BYTE**

 * **PMEM2_GRANULARITY_CACHE_LINE**

 * **PMEM2_GRANULARITY_PAGE**

A description of the granularity concept can be found in **libpmem2**(7) manpage.

# RETURN VALUE #

**pmem2_config_set_required_store_granularity**() function returns 0 on success.
Otherwise, it returns one of the following error values:

 * **PMEM2_E_INVALID_ARG** - granularity *g* is not a valid value.

# SEE ALSO #
**pmem2_config_new**(3), **libpmem2**(7)
and **<http://pmem.io>**
