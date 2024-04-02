---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem_log_set_threshold.3.html"]
title: "libpmem | PMDK"
header: "pmem API version 1.1"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2024, Intel Corporation)

[comment]: <> (pmem_log_set_threshold.3 -- set the logging threshold value)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem_log_set_threshold** - set the logging threshold value

# SYNOPSIS #

```c
	#include <libpmem.h>

	enum pmem_log_level {
		PMEM_LOG_LEVEL_HARK,
		PMEM_LOG_LEVEL_FATAL,
		PMEM_LOG_LEVEL_ERROR,
		PMEM_LOG_LEVEL_WARNING,
		PMEM_LOG_LEVEL_NOTICE,
		PMEM_LOG_LEVEL_INFO,
		PMEM_LOG_LEVEL_DEBUG
	};

	enum pmem_log_threshold {
		PMEM_LOG_THRESHOLD,
		PMEM_LOG_THRESHOLD_AUX
	};

	int pmem_log_set_threshold(enum pmem_log_threshold threshold,
		enum pmem_log_level value);
```

# DESCRIPTION #

**pmem_log_set_threshold**() sets the logging threshold value.

Available thresholds are:

 - **PMEM_LOG_THRESHOLD** - the main threshold used to filter out undesired
  logging messages. Messages less severe than indicated by this threshold's
  value are ignored so they won't make it to the logging function
  (please see **pmem_log_set_function**(3)).
  The default value is **PMEM_LOG_WARNING**.
 - **PMEM_LOG_THRESHOLD_AUX** - the auxiliary threshold intended for use inside
  the logging function (please see **pmem_log_get_threshold**(3)). A custom
  logging function may or may not take this threshold into consideration depending
  on the developer's needs. For the default logging function behaviour please see
  **pmem_log_set_function**(3). The initial value of this threshold is
  **PMEM_LOG_LEVEL_HARK**.

Available threshold values are defined by enum *pmem_log_level*.
They are listed in descending order of severity:

 - **PMEM_LOG_LEVEL_HARK** - only basic library info. Shall be used to
   effectively suppress the logging since it is used just for a few messages
   when the program is started and whenever a new logging function is set.
   Please see **pmem_log_set_function**(3).
 - **PMEM_LOG_LEVEL_FATAL** - an error that causes the program to stop working
   immediately. Sorry.
 - **PMEM_LOG_LEVEL_ERROR** - an error that causes the current operation to fail
 - **PMEM_LOG_LEVEL_WARNING** - an unexpected situation that does NOT
   cause the current operation to fail
 - **PMEM_LOG_LEVEL_NOTICE** - non-massive info mainly related to public API
   function completions
 - **PMEM_LOG_LEVEL_INFO** - massive info e.g. every write operation indication
 - **PMEM_LOG_LEVEL_DEBUG** - debug info e.g. write operation dump

# RETURN VALUE #

**pmem_log_set_threshold**() function returns 0 on success or returns
a non-zero value and sets errno on failure. On failure, the *threshold* value remains unchanged.

# ERRORS #

**pmem_log_set_threshold**() can set the following errno values on fail:

 - EINVAL - *threshold* is not **PMEM_LOG_THRESHOLD** nor
   **PMEM_LOG_THRESHOLD_AUX**
 - EINVAL - *value* is not defined by enum *pmem_log_level* type
 - EAGAIN - multiple threads attempted to change the *threshold* value concurrently.
   A retry may fix the problem. This error is not expected when the function is
   called from just one thread at a time.

# SEE ALSO #

**pmem_log_get_threshold**(3), **pmem_log_set_function**(3).
