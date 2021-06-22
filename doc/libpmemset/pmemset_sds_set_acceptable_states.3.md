---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_SDS_SET_ACCEPTABLE_STATES, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_sds_set_acceptable_states.3 -- man page for pmemset_sds_set_acceptable_states)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_sds_set_acceptable_states**() - set acceptable part states in shutdown
data state structure

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_sds;
int pmemset_sds_set_acceptable_states(struct pmemset_sds *sds, uint64_t states);

```

# DESCRIPTION #

The **pmemset_sds_set_acceptable_states**() function sets acceptable part states
in the shutdown data state *sds* structure.

Acceptable part states are compared with the determined part state during the mapping
of a new part using **pmemset_part_map**(3) function. If determined part state is not
among acceptabled states then **pmemset_part_map**(3) function terminates.

Each created part can be described by one of the following states:

* **PMEMSET_PART_STATE_INDETERMINATE** - the pool state cannot be determined because of
the errors during retrieval of device information.

* **PMEMSET_PART_STATE_OK** - the pool is internally consistent and was closed cleanly.
Application can assume that no custom recovery is needed.

* **PMEMSET_PART_STATE_OK_BUT_INTERRUPTED** - the pool is internally consistent, but it
was not closed cleanly. Application must perform consistency checking and custom recovery
on user data.

* **PMEMSET_PART_STATE_CORRUPTED** - the pool can contain invalid data as a result of hardware
failure. Reading the pool is unsafe.

**PMEMSET_PART_STATE_OK** is the default acceptable state for each new SDS structure.

# RETURN VALUE

The **pmemset_sds_set_acceptable_states**() function returns 0 on success or a negative
error code on failure.

# ERRORS #

**pmemset_sds_set_acceptable_states**() can fail with the following errors:

* **PMEMSET_E_INVALID_PART_STATES** - combination of part states to be set in SDS structure
contains a part state undefined by libpmemset library

# SEE ALSO #

**pmemset_part_map**(3),
**libpmemset**(7) and **<http://pmem.io>**
