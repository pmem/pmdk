This directory contains DML implementation *miniasync* library (*miniasync-dml*
library).

The DML library is required to compile *miniasync-dml* library.
By default DML compiles with software path only. If you want to make use of
hardware path, make sure that DML is installed with ```DML_HW``` option.

By default, *miniasync-dml* mover performs operations using software path.
To perform operation with hardware path use *MINIASYNC_DML_F_PATH_HW* flag.

Compiling *miniasync-dml*:
```shell
$ cmake .. -DCOMPILE_DML=ON
```
