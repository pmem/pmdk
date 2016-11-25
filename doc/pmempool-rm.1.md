---
layout: manual
Content-Style: 'text/css'
title: pmempool-rm(1)
header: NVM Library
date: pmem Tools version 1.2.0
...

[comment]: <> (Copyright 2016, Intel Corporation)

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

[comment]: <> (pmempool-rm.1 -- man page for pmempool-rm)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmempool-rm** -- Remove (unlink) poolset files


# SYNOPSIS #

```
$ pmempool rm [<options>] <file>..
```

# DESCRIPTION #

The **pmempool** invoked with *rm* command removes (unlinks) all files specified
in command line arguments. If the specified file is a poolset file all parts will
be removed. All files are removed using the **unlink**(3) call. Without
specifying the **-i|--interactive** option, the *rm* command prompts only before
removing *write-protected* files. If specified file does not exist the *rm* command
terminates with error code. The **-f|--force** command ignores non-existing files
and never prompts before removing a file.
See **EXAMPLES** section for example usage of the *rm* command.

##### Available options: #####

`-h, --help`

Print help message

`-v, --verbose`

Be verbose and print all removing files.

`-s, --only-pools`

Remove only pool files and do not remove poolset files.

`-f, --force`

Remove all specified files, ignore not existing files, never prompt.

`-i, --interactive`

Prompt before removing every single file.


# EXAMPLE #

```
$ pmempool rm pool.obj pool.blk
```

Remove specified pool files

```
$ pmempool rm -s pool.set
```

Remove all pool files from the "pool.set", do not remove *pool.set* itself.


# SEE ALSO #

**pmempool**(1), **libpmemlog**(3), **libpmemblk**(3), **libpmemobj**(3)
and **<http://pmem.io>**
