# Required DAOS patches

- `00_harmonize_VOS_pool_path.patch` which:
  - harmonizes the VOS pool path in tests with normal DTX engine
  - fixes both pool and container UUIDs so it is predictable
- `01_stop_using_test_args_reset.patch` which:
  - stops the use of test_args_reset()

> The use of the test_args_reset() utility breaks the setup/teardown symmetry.
If a test requires a pool/container to be recreated it has to do it in its
setup/teardown instead of relying on the test suite setup/teardown
(e.g. setup_io/teardown_io) and then calling test_args_reset() over and over
again. If a pool/container has been created in the setup, it should be destroyed
in the respective teardown.

- `02_reduce_memory_requirements.patch` which:
  - reduces the memory requirements for UTs
- `03_CMOCKA_FILTER_SUPPORTED.patch` which:
  - `CMOCKA_FILTER_SUPPORTED=1`
- `04_force_use_sys_db.patch` which:
  - forces the use of sys_db
- `05_dts_basic_create_open_close.patch` which:
  - introduces dts_basic_create/open/close
- `06_basic_dtx_ut.patch` which:
  - introduces a basic DTX life-cycle UT
- `07_disable_ABT.patch` which:
  - prevents ABT from screwing with Valgrind
- `08_cmocka_filter_fix.patch` which:
  - fixes cmocka's filter (invalid size calculation)
- `09_fix_size_t_from_int.patch` which:
  - fixes conversion to size_t from int
