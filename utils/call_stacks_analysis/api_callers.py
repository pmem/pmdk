#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation

import argparse
import json

from typing import List, Dict, Any

# https://peps.python.org/pep-0589/ requires Python >= 3.8
# from typing import TypedDict

# List all API calls which start call stacks containing a particular function name.

PARSER = argparse.ArgumentParser()
PARSER.add_argument('-a', '--all-call-stacks-file', required=True)
PARSER.add_argument('-f', '--function-name', required=True)

# class CallStack(TypedDict): # for Python >= 3.8
#     stack: list[str]
#     size: int
CallStack = Dict[str, Any] # for Python < 3.8

def load_all_call_stacks(all_call_stacks_file: str) -> List[CallStack]:
        with open(all_call_stacks_file, 'r') as file:
                return json.load(file)

def main():
        args = PARSER.parse_args()
        call_stacks = load_all_call_stacks(args.all_call_stacks_file)
        apis = []
        # lookup all call stacks in which the function of interest is mentioned
        for call_stack in call_stacks:
                if args.function_name in call_stack['stack']:
                        # callect all API calls which starts these call stacks
                        apis.append(call_stack['stack'][0])
        # remove duplicates
        apis = list(set(apis))
        apis.sort()
        for api in apis:
                print(api)

if __name__ == '__main__':
        main()
