# C Style and Coding Standards for Persistent Memory Development Kit

This document defines the coding standards and conventions for writing
PMDK code. To ensure readability and consistency within the code,
the contributed code must adhere to the rules below.

### Introduction
The Persistent Memory Development Kit coding style is quite similar to the style
used for the SunOS product.
A full description of that standard can be found
[here.](https://www.cis.upenn.edu/~lee/06cse480/data/cstyle.ms.pdf)

This document does not cover the entire set of recommendations and formatting rules
used in writing PMDK code, but rather focuses on some PMDK-specific conventions,
not described in the document mentioned above, as well as the ones the violation
of which is most frequently observed during the code review.
Also, keep in mind that more important than the particular style is **consistency**
of coding style. So, when modifying the existing code, the changes should be
coded in the same style as the file being modified.

### Code formatting
Most of the common stylistic errors can be detected by the
[style checker program](https://github.com/pmem/pmdk/blob/master/utils/cstyle)
included in the repo.
Simply run `make cstyle` to verify if your code is well-formatted.

Here is the list of the most important rules:
- The limit of line length is 80 characters.
- Indent the code with TABs, not spaces. Tab width is 8 characters.
- Do not break user-visible strings (even when they are longer than 80 characters)
- Put each variable declaration in a separate line.
- Do not use C++ comments (`//`).
- Spaces around operators are mandatory.
- No whitespace is allowed at the end of line.
- For multi-line macros, do not put whitespace before `\` character.
- Precede definition of each function with a brief, non-trivial description.
(Usually a single line is enough.)
- Use `XXX` tag to indicate a hack, problematic code, or something to be done.
- For pointer variables, place the `*` close to the variable name not pointer type.
- Avoid unnecessary variable initialization.
- Never type `unsigned int` - just use `unsigned` in such case.
Same with `long int` and `long`, etc.
- Sized types like `uint32_t`, `int64_t` should be used when there is an on-media format.
Otherwise, just use `unsigned`, `long`, etc.
- Functions with local scope must be declared as `static`.

### License & copyright
- Make sure you have the right to submit your contribution under the BSD license,
especially if it is based upon previous work.
See [CONTRIBUTING.md](https://github.com/pmem/pmdk/blob/master/CONTRIBUTING.md) for details.
- A copy of the [BSD-style License](https://github.com/pmem/pmdk/blob/master/LICENSE)
must be placed at the beginning of each source file, script or man page
(Obviously, it does not apply to README's, Visual Studio projects and \*.match files.)
- When adding a new file to the repo, or when making a contribution to an existing
file, feel free to put your copyright string on top of it.

### Naming convention
- Keep identifier names short, but meaningful. One-letter variables are discouraged.
- Use proper prefix for function name, depending on the module it belongs to.
- Use *under_score* pattern for function/variable names. Please, do not use
CamelCase or Hungarian notation.
- UPPERCASE constant/macro/enum names.
- Capitalize first letter for variables with global or module-level scope.
- Avoid using `l` as a variable name, because it is hard to distinguish `l` from `1`
on some displays.

### Multi-OS support (Linux/FreeBSD)
- Do not add `#ifdef <OS>` sections lightly. They should be treated as technical
debt and avoided when possible.
- Use `__FreeBSD__` macro for conditional directives for FreeBSD-specific code.
- In case of large portions of code (i.e. a whole function) that have different
implementation for each OS, consider moving them to separate files.
(i.e. *xxx_linux.c*, *xxx_freebsd.c* and *xxx_windows.c*)
- Keep in mind that `long int` is always 32-bit in VC++, even when building for
64-bit platforms. Remember to use `long long` types whenever it applies, as well
as proper formatting strings and type suffixes (i.. `%llu`, `ULL`).
- Standard compliant solutions should be used in preference of compiler-specific ones.
(i.e. static inline functions versus statement expressions)
- Do not use formatting strings that are not supported by Windows implementations
of printf()/scanf() family. (like `%m`)
- It is recommended to use `PRI*` and `SCN*` macros in printf()/scanf() functions
for width-based integral types (`uint32_t`, `int64_t`, etc.).

### Debug traces and assertions
- Put `LOG(3, ...)` at the beginning of each function. Consider using higher
log level for most frequently called routines.
- Make use of `COMPILE_ERROR_ON` and `ASSERT*` macros.
- Use `ERR()` macro to log error messages.

### Unit tests
- There **must** be unit tests provided for each new function/module added.
- Test scripts **must** start with `#!/usr/bin/env <shell>` for portability between Linux and FreeBSD.
- Please, see [this](https://github.com/pmem/pmdk/blob/master/src/test/README)
and [that](https://github.com/pmem/pmdk/blob/master/src/test/unittest/README)
document to get familiar with
our test framework and the guidelines on how to write and run unit tests.

### Commit messages
All commit lines (entered when you run `git commit`) must follow the common
conventions for git commit messages:
- The first line is a short summary, no longer than **50 characters,** starting
  with an area name and then a colon.  There should be no period after
  the short summary.
- Valid area names are: **pmem, pmem2, obj, blk, log, set,
  test, doc, daxio, pmreorder, pool** (for *libpmempool* and *pmempool*),
  **benchmark, examples, core** and **common** (for everything else).
- It is acceptable for the short summary to be the only thing in the commit
  message if it is a trivial change.  Otherwise, the second line must be
  a blank line.
- Starting at the third line, additional information is given in complete
  English sentences and, optionally, bulleted points.  This content must not
  extend beyond **column 72.**
- The English sentences should be written in the imperative, so you say
  "Fix bug X" instead of "Fixed bug X" or "Fixes bug X".
- Bullet points should use hanging indents when they take up more than
  one line (see example below).
- There can be any number of paragraphs, separated by a blank line, as many
  as it takes to describe the change.
- Any references to GitHub issues are at the end of the commit message.

For example, here is a properly-formatted commit message:
```
doc: fix code formatting in man pages

This section contains paragraph style text with complete English
sentences.  There can be as many paragraphs as necessary.

- Bullet points are typically sentence fragments

- The first word of the bullet point is usually capitalized and
  if the point is long, it is continued with a hanging indent

- The sentence fragments don't typically end with a period

Ref: pmem/issues#1
```
