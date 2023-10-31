#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation

import argparse
import json

from typing import TypedDict

# Print out stack usage per function call for a given call stack.

PARSER = argparse.ArgumentParser()
PARSER.add_argument('-s', '--stack-usage-file')
PARSER.add_argument('-c', '--call-stack')

class StackUsageRecord(TypedDict):
    size: int

class CallStack(TypedDict):
    stack: list[str]
    size: int

def load_stack_usage(stack_usage_file: str) -> dict[str, StackUsageRecord]:
        with open(stack_usage_file, 'r') as file:
                return json.load(file)

def load_call_stack(call_stack: str) -> CallStack:
        with open(call_stack, 'r') as file:
                return json.load(file)

def main():
        args = PARSER.parse_args()
        stack_usage = load_stack_usage(args.stack_usage_file)
        call_stack = load_call_stack(args.call_stack)
        for func in call_stack['stack']:
                if func in stack_usage.keys():
                        size = stack_usage[func]['size']
                else:
                        size = 0
                print(f"{size}\t{func}")

if __name__ == '__main__':
        main()
