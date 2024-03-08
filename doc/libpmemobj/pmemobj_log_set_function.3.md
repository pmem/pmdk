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

[comment]: <> (pmemobj_log_set_function.3 -- man page for set the logging function)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[NOTE](#note)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemobj_log_set_function**() - set the logging function

# SYNOPSIS #

```c
#include <libpmemobj.h>

	typedef void pmemobj_log_function(
		enum pmemobj_log_level level,
		const char *file_name,
		const int line_no,
		const char *function_name,
		const char *message_format,
		...);

	int pmemobj_log_set_function(pmemobj_log_function *log_function);
```

# DESCRIPTION #

**pmemobj_log_set_function**() allows choosing the function which will get all
the generated logging messages. The log_function can be either
**PMEMOBJ_LOG_USE_DEFAULT_FUNCTION** which will use the default logging function
(built into the library) or a pointer to a user-defined function.

Parameters of a user-defined log function are as follow:
- **level** - the log level of the message
- **file_name** - name of the source file where the message coming from. It could
              be set to NULL and
  in such case neither line_no nor function_name are provided.
 - **line_no** - the source file line where the message coming from
 - **function_name** - the function name where the message coming from
 - **message_format** - **printf**(3)-like format string of the message
 - "..." - additional arguments of the message format string

THE DEFAULT LOGGING FUNCTION
The initial value of the logging function is **CORE_LOG_USE_DEFAULT_FUNCTION**.
This function writes messages to **syslog**(3) (the primary destination) and to
**stderr**(3) (the secondary destination).

# RETURN VALUE #

**pmemobj_log_set_function**() function returns 0 on success or returns
a non-zero value and sets errno on failure.

# ERRORS #

 - EAGAIN - a temporary error occurred, the retry may fix the problem

# NOTE #

The logging messages on the levels above the CORE_LOG_THRESHOLD level won't
trigger the logging function.

The user defined function must be thread-safe.

# SEE ALSO #

**pmemobj_log_get_threshold**(3), **pmemobj_log_set_threshold**(3).