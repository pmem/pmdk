---
layout: manual
Content-Style: 'text/css'
title: LIBPMEM2_UNSAFE_SHUTDOWN
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020-2021, Intel Corporation)

[comment]: <> (libpmem2_unsafe_shutdown.7 -- man page for libpmem2 unsafe shutdown)

[NAME](#name)<br />
[DESCRIPTION](#description)<br />
[UNSAFE SHUTDOWN DETECTION](#unsafe-shutdown-detection)<br />
[SEE ALSO](#see-also)

# NAME #

**libpmem2_unsafe_shutdown** - libpmem2 unsafe shutdown

# DESCRIPTION #

In systems with the persistent memory support, *a power-fail protected domain*
covers a set of resources from which the platform will flush data to
*the persistent medium* in case of *a power-failure*. Data stored on
*the persistent medium* is preserved across power cycles.

The hardware guarantees the feature to flush all data stored in
*the power-fail protected domain* to *the persistent medium*. However, nothing
is infallible, and Persistent Memory hardware can expose a monotonically
increasing *unsafe shutdown counter* (**USC**) that is incremented every time
a failure of the mechanism above is detected. This allows software to discover
situations where a running application was interrupted by a power failure that
led to an unsafe shutdown. Undiscovered unsafe shutdowns might cause silent data
corruption.

>Note: *The unsafe shutdown* may corrupt data stored on a device, in a file,
in a set of files, and a mapping spanning only a part of a file.
For the sake of simplicity, all of the above cases will be called *file* below.

# UNSAFE SHUTDOWN DETECTION #

Software can detect an unsafe shutdown by watching for the change between
unsafe shutdown count value across application startups. Any changes can be
indicative of unsafe shutdown occurrence.

Applications can implement a detection mechanism by storing the **USC** retrieved
from **pmem2_source_device_usc**(3) in Persistent Memory. Then, on subsequent
startups, the stored value must be compared with a newly retrieved one.

However, this detection method can result in false-positives. Moving the file to
different Persistent Memory devices with possibly different **USC** values would
lead to false unsafe shutdown detection.

Additionally, relying on **USC** value alone could result in the detection of
unsafe shutdown events that occur when such a shutdown has no chance of impacting
the data used by the application, e.g., when nothing is actively using the file.

Applications can avoid false-positives associated with moving the file by storing
device identification, obtained through **pmem2_source_device_id**(3), alongside
the **USC**. This enables the software to check if the underlying device has
changed, and reinitialize the stored **USC** in such cases.

The second behavior, detection of possibly irrelevant unsafe shutdown events,
if undesirable, can be prevented by storing a flag indicating whether the file
is in use, alongside all the rest of the relevant information.

The application should use **pmem2_deep_flush**(3) when storing any data related
to unsafe shutdown detection for higher reliability. This helps ensure that the
detection mechanism is not reliant on the correct functioning of the same hardware
features it is designed to safeguard.

General-purpose software should not assume the presence of **USC** on the platform,
and should instead appropriately handle any *PMEM2_E_NOSUPP* it encounters.
Doing otherwise might cause the software to be unnecessarily restrictive about
the hardware it supports and would prevent, e.g., testing on emulated PMEM.

# SEE ALSO #

**pmem2_deep_flush**(3), **pmem2_persist_fn**(3), **pmem2_source_device_id**(3),
**pmem2_source_device_usc**(3) and **<https://pmem.io>**
