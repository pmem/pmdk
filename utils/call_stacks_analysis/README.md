# Call-stacks analysis utilities

> XXX This document requires more details.

1. Generate stack usage file using `stack_usage_stats.sh`.
2. Generate call stack graph using `cflow.sh`.
3. Generate all possible call stacks given the data provided.

```sh
# -u, --stack-usage-stat-file
# -f, --cflow-output-file
# -i, --config-file
./generate_call_stacks.py \
        -u stack-usage-nondebug.txt \
        -f cflow.txt \
        -e extra_calls.json \
        -w white_list.json
```

If succesfull, it produces:

- `call_stacks.json` containing call stacks whose call stack use is above the lower limit (the condition applicable only if `--lower-limit` is used) and `call_stacks_excluded.json` whose call stack use is below the limit. Both files are ordered in descending order by call stack use.
- `stack_usage.json` with the data extracted from the provided `src/stats/stack-usage-nondebug.txt`.

**Note**:  If too many functions ought to be added to a white list it might be useful to ignore functions having a certain stack usage or lower. Please see `-t` option to set a desired threshold.

4. (Optional) Break down a call stack's stack consumption per function. Use the `stack_usage.json` as produced in the previous step and extract a single call stack and put it into a file (name `call_stack.json` below). Please see the examples directory for an example.

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

5. (Optional) List all API calls which call stacks contains a given function. Use the `stack_usage.json` as produced in the previous step.

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
