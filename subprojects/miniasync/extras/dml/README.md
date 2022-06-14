This directory contains **miniasync-vdm-dml** library, a library encapsulating **DML**
implementation of the **miniasync_vdm** virtual data mover.

The *Data Mover Library* (**DML**) is required to compile **miniasync-vdm-dml** library.
By default **DML** compiles with software path only. If you want to make use of
hardware path, make sure that DML is installed with **DML_HW** option.

**DML** data mover supports offloading certain computations to the hardware
accelerators (e.g. Intel® Data Streaming Accelerator). To use this feature, make
sure that **DML** library is compiled with **DML_HW** option.
For more information about **DML**, see [DML](https://github.com/intel/DML).

Compiling *miniasync-vdm-dml*:
```shell
$ cmake .. -DCOMPILE_DML=ON
```
