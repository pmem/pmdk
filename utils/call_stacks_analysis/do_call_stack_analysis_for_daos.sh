#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025, Hewlett Packard Enterprise Development LP
#
#
# Execute complete call stack analysis filtered for DAOS only function calls.
#
# Update the examples/api_filter.txt file based on the actual DAOS source code. (Optional)
# grep -r -E 'pmemobj_[^(]*\(' $daos_src_folder | grep -v vos_pmemobj | \
#      sed 's/.*\(pmemobj_[^)]*(\).*/\1/p' | sed 's/(.*//' |  sort | uniq
#

function help() {
        echo "usage:"
        echo "       $0 [-d|--daos]"
        echo
        echo "       -d/--daos use the actual pmemobj API calls from the DAOS source code"
        echo "       -h/--help print help"
        echo
}

function main() {
        local update=$1
        local filter_name="examples/api_filter.txt"

        if [ $update == 1 ]; then
                wget -q https://github.com/daos-stack/daos/archive/refs/heads/master.zip -O daos.zip
                unzip -q -u daos.zip
                rm daos.zip
                grep -r -E -h -o 'pmemobj_[^(]*\(' 'daos-master/src' | sed 's/(.*$//' | sort | uniq > $filter_name
                rm -rf daos-master
        fi

        ./make_stack_usage.sh && \
                ./make_api.sh && \
                ./make_extra.py && \
                ./make_cflow.sh && \
                ./make_call_stacks.py --filter-api-file $filter_name --filter-lower-limit 11568 \
                        --dump-all-stacks
}

if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        help
        exit 1
fi

if [[ "$1" == "-d" || "$1" == "--daos" ]]; then
        daos=1
else
        daos=0
fi

main $daos
