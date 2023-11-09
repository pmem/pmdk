# Call-stacks analysis utilities

> XXX This document requires more details.

## Pre-requisites

- built PMDK
- `cflow` command available in the system. Available [here](https://www.gnu.org/software/cflow/).

## Generating call stacks

```sh
./make_stack_usage.sh
./make_api.sh
./make_extra.py
./make_cflow.sh
./generate_call_stacks.py
```

If succesfull, it produces:

- `call_stacks_all.json` containing all found call stacks.
- `call_stacks_final.json` containing all call stacks that met criteria (e.g. `--lower-limit` and/or `--api-filter-file`).
- `stack_usage.json` with the data extracted from the provided `src/stats/stack-usage.txt`.

`call_stacks_*.json` files are ordered in descending order by call stack use.

**Note**:  If too many functions ought to be added to a white list it might be useful to ignore functions having a certain stack usage or lower. Please see `-t` option to set a desired threshold.

## Optional

### Call stack's stack consumption per function

Use the `stack_usage.json` as produced in the previous step and extract a single call stack and put it into a file (name `call_stack.json` below). Please see the examples directory for an example.

```sh
# -s, --stack-usage-file
# -c, --call-stack
./utils/call_stacks_analysis/stack_usage.py \
        -s stack_usage.json \
        -c call_stack.json
```

If successful, it prints out on the screen a list of functions along with their stack consumption e.g.

```
208     pmem_map_file
0       pmem_map_fileU
80      pmem_map_register
64      util_range_register
240     util_ddax_region_find
8224    pmem2_get_type_from_stat
0       ERR
384     out_err
0       out_error
224     out_snprintf
```

### List all API calls which call stacks contains a given function

Use the `stack_usage.json` as produced in the previous step.

```sh
# -a, --all-call-stacks-file
# -f, --function-name
./utils/call_stacks_analysis/api_callers.py \
        -a call_stacks_all.json \
        -f pmem2_get_type_from_stat
```

If successful, it prints out on screen a list of API calls that met the condition e.g.

```
os_part_deep_common
pmem_map_file
util_fd_get_type
util_file_device_dax_alignment
util_file_pread
util_file_pwrite
util_unlink_flock
```
