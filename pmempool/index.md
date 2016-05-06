---
title: pmempool
layout: nvml
---

#### The pmempool utility

**pmempool** is a standalone utility for management and off-line analysis
of persistent memory pools. It works for both the *single-file* pools and
for *pool set files*. Below you can find a list of available commands.

See the [pmempool man page](master/pmempool.1.html) page for HEAD documentation and exmaples or see older versions:

* Man page for [pmempool version 1.0](v1.0/pmempool.1.html)

##### create

The **pmempool create** command creates a persistent memory pool of specified
type and parameters. Creating the persistent memory pool is possible using
the NVM library APIs, however using the **pmempool** utility for creating the
pool files may simplify the user application.

See the [pmempool create man page](master/pmempool-create.1.html) for HEAD documentation
and examples or see older versions:

* Man page for [pmempool create version 1.0](v1.0/pmempool-create.1.html)

##### info

The **pmempool info** command performs an off-line analysis of a persistent memory
pool. The **pmempool info** command prints all persistent data
structures and evaluates some basic statistics of the pool. By default the
**pmempool info** prints the *pool header* and *pool descriptor* data
structures, but by adding command line arguments it is possible to print
additional information about the pool.

See the [pmempool info man page](master/pmempool-info.1.html) for HEAD documentation
and examples or see older versions:

* Man page for [pmempool info version 1.0](v1.0/pmempool-info.1.html)

##### dump

The **pmempool dump** command dumps the user data stored in a persistent memory
pool file. This command currently works only for the **pmemblk** and
**pmemlog** pool types. It is possible to pick the data dump format as well as
the range of desired data.

See the [pmempool dump man page](master/pmempool-dump.1.html) for HEAD documentation
and examples or see older versions:

* Man page for [pmempool dump version 1.0](v1.0/pmempool-dump.1.html)

##### check

The **pmempool check** command checks consistency of the persistent memory pool.
It prints information about errors found. This command is able to repair
a broken pool in some cases. Currently it works only for the **pmemblk** and
**pmemlog** pool types but the support for **pmemobj** pool type will be
available in the near feature.

See the [pmempool check man page](master/pmempool-check.1.html) for HEAD documentation
and examples or see older version:

* Man page for [pmempool check version 1.0](v1.0/pmempool-check.1.html)

##### rm

The **pmempool rm** is a simple replacement for the system **rm** command for
the persistent memory pools. The command may be useful for removing the
*pool set files* because it removes all part files specified in the
configuration file. For *single-file* pools it works almost the same
as the standard system **rm** command.

See the [pmempool rm man page](master/pmempool-rm.1.html) for HEAD documentation
and examples or see older version:

* Man page for [pmempool rm version 1.0](v1.0/pmempool-rm.1.html)
