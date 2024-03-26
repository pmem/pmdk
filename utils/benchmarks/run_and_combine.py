#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

import argparse
import pandas as pd
import subprocess

from typing import List, Dict, Any


PARSER = argparse.ArgumentParser()
PARSER.add_argument('--reference', metavar='LD_LIBRARY_PATH', required=True,
        help='LD_LIBRARY_PATH where the first version of PMDK is built')
PARSER.add_argument('--rival', metavar='LD_LIBRARY_PATH', required=True,
        help='LD_LIBRARY_PATH where the second version of PMDK is built')
PARSER.add_argument('-c', '--config', required=True,
        help='Name of the .cfg file to use')
PARSER.add_argument('-s', '--scenario', required=True,
        help='Name of the scenario to run')
PARSER.add_argument('-p', '--pmem_path', required=True,
        help='PMEM-mounted directory to use')
PARSER.add_argument('-n', '--numa_node', required=True, help='NUMA node to use')


IDX_TO_NAME = ['ref', 'riv']


def output_name(args: argparse.Namespace, idx: int) -> str:
        """Generate a file name for an output ref or riv"""
        return f'{args.config}__{args.scenario}_{IDX_TO_NAME[idx]}.csv'


def run(args: argparse.Namespace, idx: int, ld_library_path: str) -> None:
        """Run PMEMBENCH according to the provided parameters"""
        config = f'src/benchmarks/{args.config}.cfg'
        file = f'{args.pmem_path}/testfile.obj'
        cmd = f'numactl --cpunodebind {args.numa_node} --localalloc ./src/benchmarks/pmembench {config} {args.scenario} --file {file}'
        env = {'LD_LIBRARY_PATH': ld_library_path}
        result = subprocess.run(cmd, env=env, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, encoding='utf-8', shell=True)
        # generate the file with the output
        if result.returncode == 0:
                # drop the first line e.g. obj_rbtree_map_insert: map_insert [1]
                out = ''.join(result.stdout.splitlines(keepends=True)[1:])
        else:
                out = result.stdout
        with open(output_name(args, idx), 'w') as output:
                output.write(out)
        # validate the run
        if result.returncode != 0:
                print(result.stdout)
                print(result.stderr)
                exit(result.returncode)


COLUMNS_COMBINE = [
        'total-avg[sec]',
        'ops-per-second[1/sec]',
        'total-max[sec]',
        'total-min[sec]',
        'total-median[sec]',
        'total-std-dev[sec]',
        'latency-avg[nsec]',
        'latency-min[nsec]',
        'latency-max[nsec]',
        'latency-std-dev[nsec]',
        'latency-pctl-50.0%[nsec]',
        'latency-pctl-99.0%[nsec]',
        'latency-pctl-99.9%[nsec]',
]


COLUMNS_COPY = [
        'threads',
        'ops-per-thread',
        'data-size',
        'seed',
        'repeats',
        'thread-affinity',
        'main-affinity',
        'min-exe-time',
        'random'
        'min-size'
        'type-number',
        'operation',
        'lib'
        'min-size',
        'lib',
        'nestings',
        'type',
        'max-key',
        'external-tx',
        'alloc',
]


def column_name(column: str, idx: int) -> str:
        """Generate a column name with ref or rev infix"""
        return f'-{IDX_TO_NAME[idx]}['.join(column.split('['))


def combine(args: argparse.Namespace) -> None:
        """"
        Combine outputs from the reference and rival runs.

        Output data files:
        - combined - contains data from both ref and riv, and a normalized
          difference between them.
        - diff - just a normalized difference between ref and riv
        """
        dfs = [pd.read_csv(output_name(args, idx), sep=';') for idx in range(2)]
        combined = pd.DataFrame()
        diff = pd.DataFrame()
        for column in COLUMNS_COMBINE:
                # Copy columns to combine from both ref and riv
                for idx in range(2):
                        combined[column_name(column, idx)] = dfs[idx][column]
                diff_column = f'{column}-diff'
                # Normalized difference between ref and riv:
                # diff = (riv - ref) / ref
                # Both output data frames contains diff columns.
                combined[diff_column] = (dfs[1][column] / dfs[0][column] - 1)
                diff[diff_column] = combined[diff_column]
        for column in COLUMNS_COPY:
                if column in dfs[0].columns:
                        # These columns are identical in both data frames
                        # so they can be copied from either data frame ref or riv.
                        combined[column] = dfs[0][column]
                        diff[column] = dfs[0][column]
        # Write the generated data frames to CSV files.
        prefix = f'{args.config}__{args.scenario}'
        combined.to_csv(f'{prefix}_combined.csv', index=False, float_format='%.3f')
        diff.to_csv(f'{prefix}_diff.csv', index=False, float_format='%.3f')


def main():
        args = PARSER.parse_args()
        for idx, ld_library_path in enumerate([args.reference, args.rival]):
                run(args, idx, ld_library_path)
        combine(args)


if __name__ == '__main__':
        main()
