# C Style and Coding Standards for NVM Library

This document defines the coding standards and conventions for writing
NVM Library code. To ensure readability and consistency within the code,
the contributed code must adhere to the rules below.

### Introduction
The NVM Library coding style is very similar to the style used for the SunOS product.
A full description of that standard can be found
[here.](https://www.cis.upenn.edu/~lee/06cse480/data/cstyle.ms.pdf)

This document does not cover the entire set of recommendations and formatting rules
used in writing NVML code, but rather focuses on some NVML-specific conventions,
not described in the document mentioned above.
Also, keep in mind that more important than the particular style is **consistency**
of coding style. So, when modifying the existing code, the changes should be
coded in the same style as the file being modified.

### Code formatting
Most of the common stylistic errors can be detected by the
[style checker program](https://github.com/pmem/nvml/blob/master/utils/cstyle)
included in the repo.
Simply run `make cstyle` or `CSTYLE.ps1` to verify if your code is well-formatted.

Here is the list of the most important rules:
- The limit of line length is 80 characters.
- Indent the code with TABs, not spaces. Tab stop is 8 characters.
- Put each variable declaration in a separate line.
- Do not use C++ comments (`//`).
- Spaces around operators are mandatory.
- No whitespace is allowed at the end of line.
- For multi-line macros, do not put whitespace before `\` character.
- Precede definition of each function with a brief description. (Usually a single
line is enough.)
- Use `XXX` tag to indicate a hack, problematic code, or something to be done.
- For pointer variables, place the `*` close to the variable name not pointer type.
- Avoid unnecessary variable initialization.
- Never type `unsigned int` - just use `unsigned` in such case.
Same with `long int` and `long`, etc.
- Sized types like `uint32_t`, `int64_t` should be used when there is an on-media format.
Otherwise, just use `unsiged`, `long`, etc.
- Functions with local scope must be declared as `static`.

### License & copyright
- Make sure you have the right to submit your contribution under the BSD license,
especially if it is based upon previous work.
See [CONTRIBUTING.md](https://github.com/pmem/nvml/blob/master/CONTRIBUTING.md) for details.
- A copy of the [BSD-style License](https://github.com/pmem/nvml/blob/master/LICENSE)
must be placed at a the beginning of each source file, makefile or script.
(Obviously, it does not apply to README's, Visual Studio projects and match files.)
- When adding a new file to the repo, or when making a contribution to an existing
file, feel free to put your copyright string on top of it.

### Naming convention
- Keep identifier names short, but meaningful.
- Use proper prefix for function name, depending on the module it belongs to.
- Use *under_score* pattern for function/variable names. Please, do not use CamelCase
nor Hungarian notation.
- UPPERCASE constant/macro/enum names.
- Capitalize first letter for variables with global or module-level scope.
- Avoid using `l` as a variable name.

### Multi-OS support (Linux/Windows)
- Use `_WIN32` macro for conditional directives when including code using
Windows-specific API.
- Use `_MSC_VER` macro for conditional directives when including code using VC++
or gcc specific extensions.
- In case of large portions of code (i.e. a whole function) that have different
implementation for each OS, consider moving them to a separate files.
(i.e. *xxx_linux.c* and *xxx_windows.c*)
- Keep in mind that `long int` is still 32-bit on 64-bit Windows. Remember to
use `long long int` type whenever it applies, as well as proper formatting
strings and type suffixes (`%llu`, `ULL`).
- Avoid compiler-specific extensions, built-ins, etc.
- Selected formatting strings are not supported by Windows implementations
of printf()/scanf() family. I.e. avoid using `%j` or `%m`.

### Debug traces and assertions
- Put `LOG(3, ...)` at the beginning of each function. Consider using higher
log level for most frequently called routines.
- Make use of `COMPILE_ERROR_ON` and `ASSERT*` macros.
- Use `ERR()` macro to log error messages.

### Unit tests
- There **must** be unit tests provided for each new function/module added.
- Please, see [this](https://github.com/pmem/nvml/blob/master/src/test/README)
and [that](https://github.com/pmem/nvml/blob/master/src/test/unittest/README)
document to get familiar with
our test framework and the guidelines on how to write and run unit tests.
