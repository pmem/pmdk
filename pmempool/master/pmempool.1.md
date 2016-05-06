---
layout: manual
Content-Style: 'text/css'
title: pmempool(1)
...

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[OPTIONS](#options)<br />
[COMMANDS](#commands)<br />
[SEE ALSO](#see-also)<br />

### NAME ###

**pmempool** − Persistent Memory Pool Management Tool

### SYNOPSIS ###

```
pmempool [–help] [–version] <command> [<args>]
```

### DESCRIPTION ###

The **pmempool** is a management tool for *Persistent Memory* pool files created by **NVML** libraries.

The main purpose of **pmempool** is to provide a user with a set of utilities for off-line analysis and manipulation of pools created by pmem libraries. The pmempool is a generic command which consists of subcommands for specific purposes. Some of subcommands are required to work *without* any impact on processed pool, but some of them *may* create a new or modify an existing one.

The **pmempool** may be useful for troubleshooting by system administrators and for software developers who work on applications based on **NVML** library. The latter may find these tools useful for testing and debugging purposes also.

### OPTIONS ###

`-v, –version`

: Prints the version of **pmempool.**

`-h, –help`

: Prints synopsis and list of commands.

### COMMANDS ###

Currently there is a following set of commands available:

**pmempool-info(1)**

: Prints information and statistics in human-readable format about specified pool.

**pmempool-check(1)**

: Checks pool’s consistency and repairs pool if it is not consistent.

**pmempool-create(1)**

: Creates a pool of specified type with additional properties specific for this type of pool.

**pmempool-dump(1)**

: Dumps usable data from pool in hexadecimal or binary format.

**pmempool-rm(1)**

: Removes pool file or all pool files listed in poolset configuration file.

In order to get more information about specific *command* you can use **pmempool help <command>.**

### SEE ALSO ###

**libpmemlog(3) libpmemblk(3) libpmemobj(3)**
