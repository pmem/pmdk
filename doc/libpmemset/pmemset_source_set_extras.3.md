---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_SOURCE_SET_EXTRAS, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_source_set_extras.3 -- man page for pmemset_source_set_extras)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_source_set_extras**() - store extra parameters in the source structure

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_extras {
	struct pmemset_sds *in_sds;
	struct pmemset_badblock *in_bb;
	enum pmemset_part_state *out_state;
};

struct pmemset_source;
void pmemset_source_set_extras(struct pmemset_source *src,
		struct pmemset_extras *ext);
```

# DESCRIPTION #

The **pmemset_source_set_extras**() stores extra parameters *ext* in the source
*src* structure.

Shutdown data state parameter *in_sds* can be initialized using **pmemset_sds_new**(3)
function.

Bad block parameter *in_bb* is not yet supported.

When creating a new mapping using **pmemset_part_map**(3) function from a source with
*out_state* set in *ext*, a current part state will be returned via *out_state* variable.

**pmemset_source_set_extras**() sets *in_sds* and *in_bb* variables to NULL.

# RETURN VALUE

The **pmemset_source_set_extras**() function does not return any value.

# SEE ALSO #

**pmemset_part_map**(3), **pmemset_sds_new**(3),
**libpmemset**(7) and **<http://pmem.io>**
