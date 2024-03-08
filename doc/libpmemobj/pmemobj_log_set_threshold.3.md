---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pobj_log_set_function.3.html"]
title: "libpmemobj | PMDK"
header: "pmemobj API version 2.3"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2024, Intel Corporation)

[comment]: <> (pmemobj_log_set_threshold.3 -- man page for set the logging threshold level)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemobj_log_set_threshold** - set the logging threshold level

 # SYNOPSIS #

```c
#include <libpmemobj.h>

	int pmemobj_log_set_threshold(enum pmemobj_log_threshold threshold,
              enum pmemobj_log_level level);

	enum log_level {
		PMEMOBJ_LOG_LEVEL_HARK,
		PMEMOBJ_LOG_LEVEL_FATAL,
		PMEMOBJ_LOG_LEVEL_ERROR,
		PMEMOBJ_LOG_LEVEL_WARNING,
		PMEMOBJ_LOG_LEVEL_NOTICE,
		PMEMOBJ_LOG_LEVEL_INFO,
		PMEMOBJ_LOG_LEVEL_DEBUG,
	};

	enum log_threshold {
		PMEMOBJ_LOG_THRESHOLD,
		PMEMOBJ_LOG_THRESHOLD_AUX,
		PMEMOBJ_LOG_THRESHOLD_MAX
	};
```

# DESCRIPTION #

**pmemobj_log_set_threshold**() sets the logging threshold level.

Available thresholds are:
 - **PMEMOBJ_LOG_THRESHOLD** - the main threshold used to filter out undesired
  logging messages. Messages on a higher level than the primary threshold
  level are ignored. **PMEMOBJ_LOG_LEVEL_HARK** shall be used to suppress
 logging.
 The default value is **PMEMOBJ_LOG_WARNING**.
 - **PMEMOBJ_LOG_THRESHOLD_AUX** - the auxiliary threshold intended for use inside
  the logging function (please see **log_get_threshold**(3)). The logging
  function may or may not take this threshold into consideration. The default
  value is **PMEMOBJ_LOG_LEVEL_HARK**.

Available threshold levels are defined by enum log_level:
 - **PMEMOBJ_LOG_LEVEL_HARK** - only basic library info
 - **PMEMOBJ_LOG_LEVEL_FATAL** - an error that causes the library to stop working
   immediately
 - **PMEMOBJ_LOG_LEVEL_ERROR** - an error that causes the library to stop working
   properly
 - **PMEMOBJ_LOG_LEVEL_WARNING** - an error that could be handled in the upper
   level
 - **PMEMOBJ_LOG_LEVEL_NOTICE** - non-massive info mainly related to public API
   function completions
 - **PMEMOBJ_LOG_LEVEL_INFO** - massive info e.g. every write operation indication
 - **PMEMOBJ_LOG_LEVEL_DEBUG** - debug info e.g. write operation dump

THE DEFAULT LOGGING FUNCTION
The default logging function writes messages to **syslog**(3) and to **stderr**(3),
where **syslog**(3) is the primary destination (**PMEMOBJ_LOG_THRESHOLD** applies)
whereas **stderr**(3) is the secondary destination (**PMEMOBJ_LOG_THRESHOLD_AUX**
applies).

# RETURN VALUE #

**pmemobj_log_set_threshold**() function returns 0 on success or returns
a non-zero value and sets errno on failure.

# ERRORS #

**pmemobj_log_set_threshold**() can set the following errno values on fail:
 - EINVAL - threshold is not **PMEMOBJ_LOG_THRESHOLD** nor
   **PMEMOBJ_LOG_THRESHOLD_AUX**
 - EINVAL - level is not a value defined by enum log_level type
 - EAGAIN - a temporary error occurred, the retry may fix the problem

# SEE ALSO #

**pmemobj_log_get_threshold**(3), **pmemobj_log_set_function**(3).