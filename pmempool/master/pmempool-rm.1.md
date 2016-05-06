---
layout: manual
Content-Style: 'text/css'
title: pmempool-rm(1)
...

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLES](#examples)<br />
[SEE ALSO](#see-also)<br />
[PMEMPOOL](#pmempool)<br />


### NAME ###

**pmempool-rm** − Remove (unlink) poolset files

### SYNOPSIS ###

```
pmempool rm [<options>] <file>..
```

### DESCRIPTION ###

The **pmempool** invoked with `rm` command removes (unlinks) all files specified in command line arguments. If the specified file is a poolset file all parts will be removed. All files are removed using the **unlink (3)** call. Without specifying the `-i|–interactive` option, the `rm` command prompts only before removing *write-protected* files. If specified file does not exist the `rm` command terminates with error code. The `-f|–force` command ignores non-existing files and never prompts before removing a file. See **EXAMPLES** section for example usage of the `rm` command.

##### Available options: #####

`-h, –help`

: Print help message

`-v, –verbose`

: Be verbose and print all removing files.

`-s, –only-pools`

: Remove only pool files and do not remove poolset files.

`-f, –force`

: Remove all specified files, ignore not existing files, never prompt.

`-i, –interactive`

: Prompt before removing every single file.


### EXAMPLES ###

`pmempool rm pool.obj pool.blk`

: Remove specified pool files

`pmempool rm -s pool.set`

: Remove all pool files from the pool.set, do not remove pool.set itself.


### SEE ALSO ###

**pmempool(1)**, **pmempool-create(1)**


### PMEMPOOL ###

Part of the **pmempool(1)** suite.
