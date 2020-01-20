# Contributing to the Persistent Memory Development Kit

Down below you'll find instructions on how to contribute to the
Persistent Memory Development Kit.

Your contributions are most welcome!  You'll find it is best to begin
with a conversation about your changes, rather than just writing a bunch
of code and contributing it out of the blue.
There are several good ways to suggest new features, offer to add a feature,
or just begin a dialog about the Persistent Memory Development Kit:

* Open an issue in our [GitHub Issues Database](https://github.com/pmem/issues/issues)
* Suggest a feature, ask a question, start a discussion, etc. in our [pmem Google group](https://groups.google.com/group/pmem)
* Chat with members of the PMDK team real-time on the **#pmem** IRC channel on [OFTC](https://www.oftc.net)

**NOTE: If you do decide to implement code changes and contribute them,
please make sure you agree your contribution can be made available
under the [BSD-style License used for the Persistent Memory Development Kit](https://github.com/pmem/pmdk/blob/master/LICENSE).**

**NOTE: Submitting your changes also means that you certify the following:**

```
Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```

In case of any doubt, the gatekeeper may ask you to certify the above in writing,
i.e. via email or by including a `Signed-off-by:` line at the bottom
of your commit comments.

To improve tracking of who is the author of the contribution, we kindly ask you
to use your real name (not an alias) when committing your changes to the
Persistent Memory Development Kit:
```
Author: Random J Developer <random@developer.example.org>
```

### Code Contributions

Please feel free to use the forums mentioned above to ask
for comments & questions on your code before submitting
a pull request.  The Persistent Memory Development Kit project uses the common
*fork and merge* workflow used by most GitHub-hosted projects.
The [Git Workflow blog article](https://pmem.io/2014/09/09/git-workflow.html)
describes our workflow in more detail.

#### Linux/FreeBSD

Before contributing please remember to run:
```
	$ make cstyle
```

This will check all C/C++ files in the tree for style issues. To check C++
files you have to have clang-format version 6.0, otherwise they will be
skipped. If you want to run this target automatically at build time, you can
pass CSTYLEON=1 to make. If you want cstyle to be run, but not fail the build,
pass CSTYLEON=2 to make.
There is also a target for automatic C++ code formatting, to do this run:
```
	$ make format
```

There are cases, when you might have several clang-format-X.Y binaries and either
no clang-format or it pointing to an older version. In such case run:
```
	$ make CLANG_FORMAT=/path/to/clang-format cstyle|format
```

#### Windows

On Windows to check the code for style issues, please run:
```
	$ pmdk\utils\CSTYLE.ps1
```

To check or format C++ files, you may use a standalone Visual Studio plugin
for clang-format.  The plugin installer can be downloaded from
[LLVM Builds](https://llvm.org/builds) page.

If you are actively working on an PMDK feature, please let other
developers know by [creating an issue](https://github.com/pmem/issues/issues).
Use the template `Feature` and assign it to yourself (due to the way
GitHub permissions work, you may have to ask a team member to assign it to you).

### Bug Reports

Bugs for the PMDK project are tracked in our
[GitHub Issues Database](https://github.com/pmem/issues/issues).

When reporting a new bug, please use `New issue` button, pick proper template and fill
in all fields. Provide as much information as possible, including the product version:

#### PMDK version

Put the release name of the version of PMDK running when the
bug was discovered in a bug comment.  If you saw this bug in multiple PMDK
versions, please put at least the most recent version and list the others
if necessary.
- Stable release names are in the form `#.#` (where `#` represents
  an integer); for example `0.3`.
- Release names from working versions look like `#.#+b#` (adding a build #)
  or `#.#-rc#` (adding a release candidate number)
If PMDK was built from source, the version number can be retrieved
from git using this command: `git describe`

For binary PMDK releases, use the entire package name.
For RPMs, use `rpm -q pmdk` to display the name.
For Deb packages, run `dpkg-query -W pmdk` and use the
second (version) string.

#### Priority

Requested priority describes the urgency to resolve a defect and establishes
the time frame for providing a verified resolution. Priorities are defined as:

* **P1**: Showstopper bug, requiring a resolution before the next release of the
library.
* **P2**: High-priority bug, requiring a resolution although it may be decided
that the bug does not prevent the next release of the library.
* **P3**: Medium-priority bug.  The expectation is that the bug will be
evaluated and a plan will be made for when the bug will be resolved.
* **P4**: Low-priority bug, the least urgent.  Fixed when the resources are available.

### Other issues

On our issues page we also gather feature requests and questions. Templates to use
are `Feature` and `Question`, respectively.  They should help deliver a meaningful
description of a feature or ask a question to us (remember though we have
different means of communication, as described at the top of the page).
