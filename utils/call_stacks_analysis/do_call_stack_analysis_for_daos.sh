#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025, Hewlett Packard Enterprise Development LP
#
#
# Execute complete call stack analysis filtrered for DAOS only funcion calls
#
# update examples/api_filter.txt file based on actual DAOS source code using:
# grep -r -E 'pmemobj_[^(]*\(' $daos_src_folder | grep -v vos_pmemobj | \
#      sed 's/.*\(pmemobj_[^)]*(\).*/\1/p' | sed 's/(.*//' |  sort | uniq
#

function help() {
        echo "usage:"
        echo "       $0 [-u|--update]"
        echo
        echo "       -u/--update       use actual pmemobj api call from DAOS source code repo"
        echo
}

function main() {
        local update=${1:-0}
        local filter_name="examples/api_filter.txt"

        if [ $update == 1 ]; then
                wget -q https://github.com/daos-stack/daos/archive/refs/heads/master.zip -O daos.zip
                unzip -q -u daos.zip; rm daos.zip
                grep -r -E 'pmemobj_[^(]*\(' "daos-master/src" | grep -v vos_pmemobj | \
                        sed 's/.*\(pmemobj_[^)]*(\).*/\1/p' | sed 's/(.*//' | \
                        sort | uniq > $filter_name
                rm -rf daos-master
        fi

        ./make_stack_usage.sh && \
        ./make_api.sh && \
        ./make_extra.py && \
        ./make_cflow.sh && \
        ./make_call_stacks.py --filter-api-file $filter_name --filter-lower-limit 11568
        --dump-all-stacks
}

if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        help
        exit 1
fi

if [[ "$1" == "-u" || "$1" == "--update" ]]; then
        update=1
else
        update=0
fi

main $update