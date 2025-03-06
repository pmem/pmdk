#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025, Hewlett Packard Enterprise Development LP
#
#
# Execute complete call stack analysis based on the API subset defined in examples/api_filter.txt.
#
# Update the examples/api_filter.txt file based on the actual DAOS source code. (Optional)
#

function help() {
        if [ -n "$1" ]; then
                echo "Error: $1"
                echo
        fi
        echo "usage:"
        echo "       $0 [OPTIONS]"
        echo
        echo "OPTIONS:"
        echo "  -h, --help         print help"
        echo "  -d, --daos         use the actual pmemobj API calls from the DAOS source code"
        echo "  -f, --filter FILE  custom API filter file"
        echo
        exit 1
}

function main() {
        local update=$1
        local filter_name="$2"
        if [ $update == 1 ]; then
                wget -q https://github.com/daos-stack/daos/archive/refs/heads/master.zip -O daos.zip
                unzip -q -o daos.zip
                rm daos.zip
                grep -r -E -h -o 'pmemobj_[^(]*\(' 'daos-master/src' | sed 's/(.*$//' | \
                        sort | uniq > $filter_name
                rm -rf daos-master
        fi

        # The lower limit comes up from the DAOS memory requirements.
        # 16kB - 4kB - 720B = 11568B
        # 16kB = Stack allocated for a single Argobot's ULT
        #  4kB = a maximum DAOS' stack usage up to calling a PMDK API calls
        #  720B = safety margin
        #    ~ = Some OSes, e.g. Ubuntu 22.04, generate call stacks of size
        # a little bit over the exact limit which is not deemed a problem at the moment.

        ./make_stack_usage.sh && \
                ./make_api.sh && \
                ./make_extra.py && \
                ./make_cflow.sh && \
                ./make_call_stacks.py --filter-api-file $filter_name --filter-lower-limit 11568 \
                        --dump-all-stacks
        return $?
}

# Parse options
update=0
filter_name="examples/api_filter.txt"
while [[ "$#" -gt 0 ]]; do
  case "$1" in
    -h|--help)
      help
      ;;
    -d|--daos)
      update=1
      ;;
    -f|--filter)
      filter_name="$2"
      shift
      ;;
    *)
      help "Unknown option: $1"
      ;;
  esac
  shift
done

if [ -z "$filter_name" ]; then
        HELP "File parameter is missing"
fi

main $update $filter_name

exit $?
