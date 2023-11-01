#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation

import argparse
import json
import re

from typing import List, Dict, Any

# https://peps.python.org/pep-0589/ requires Python >= 3.8
# from typing import TypedDict
# https://peps.python.org/pep-0613/ requires Python >= 3.10
# from typing import TypeAlias


PARSER = argparse.ArgumentParser()
PARSER.add_argument('-u', '--stack-usage-stat-file', required=True)
PARSER.add_argument('-f', '--cflow-output-file', required=True)
PARSER.add_argument('-i', '--config-file', required=True)
PARSER.add_argument('-d', '--dump', action='store_true', help='Dump debug files')
PARSER.add_argument('-t', '--skip-threshold', type=int, default=0,
        help='Ignore non-reachable function if its stack usage <= threshold')

# class Config(TypedDict): # for Python >= 3.8
#     filter: str
#     api: list[str]
#     dead_end: list[str]
#     extra_calls: dict[str, list[str]]
#     white_list: list[str]
Config = Dict[str, Any] # for Python < 3.8

# class StackUsageRecord(TypedDict): # for Python >= 3.8
#     size: int
#     type: str
StackUsageRecord = Dict[str, Any] # for Python < 3.8

# StackUsage: TypeAlias = dict[str, StackUsageRecord] # for Python >= 3.10
StackUsage = Dict[str, StackUsageRecord] # for Python < 3.10

# Calls: TypeAlias = dict[str, list[str]] # for Python >= 3.10
Calls = Dict[str, List[str]] # for Python < 3.10

# RCalls: TypeAlias = Calls # for Python >= 3.10
RCalls = Calls # for Python < 3.10

# class CallStack(TypedDict): # for Python >= 3.8
#     stack: list[str]
#     size: int
CallStack = Dict[str, Any] # for Python < 3.8

DUMP = False

def dump(var, name: str, force: bool = False) -> None:
        global DUMP
        if not force and not DUMP:
                return
        with open(f'{name}.json', 'w') as outfile:
                json.dump(var, outfile, indent = 4)

def load_config(config_file: str) -> Config:
        with open(config_file, 'r') as file:
                return json.load(file)

def parse_stack_usage(stack_usage_stat_file: str, filter: str) -> StackUsage:
        funcs = {}
        with open(stack_usage_stat_file, 'r') as file:
                for line in file:
                        # 8432 out_common : src/nondebug/libpmem/out.su:out.c dynamic,bounded
                        found = re.search('([0-9]+) ([a-zA-Z0-9_]+)(.[a-z0-9.]+)* : (src/(non)?debug/)([a-z0-9]+)[a-z0-9.:/_]+ ([a-z,]+)', line)
                        if found:
                                if found.group(6) == filter:
                                        funcs[found.group(2)] = {'size': int(found.group(1)), 'type': found.group(7)}
                        else:
                                print(f'An unexpected line format: {line}')
                                exit(1)
        return funcs

def parse_cflow_output(cflow_output_file: str) -> Calls:
        calls = {}
        call_stack = []
        with open(cflow_output_file, 'r') as file:
                # processing line-by-line
                for line in file:
                        line_copy = line
                        # determine the level of nesting
                        level = 0
                        while line[0] == ' ':
                                level += 1
                                line = line[4:] # remove the prepended spaces
# pmem_memset_persist() <void *pmem_memset_persist (void *pmemdest, int c, size_t len) at pmem.c:731>:
                        found = re.search('^([a-zA-Z0-9_]+)\(\)', line)
                        if not found:
                                print('An unexpected line format:')
                                print(line_copy)
                                exit(1)
                        func = found.group(1)
                        # construct the call stack being currently processed
                        call_stack.insert(level, func)
                        # being level 0 it does not have a caller
                        if level == 0:
                                continue
                        callee = func
                        caller = call_stack[level - 1]
                        if caller in calls.keys():
                                calls[caller].append(callee)
                        else:
                                calls[caller] = [callee]
        # remove duplicate callees
        calls_unique = {}
        for k, v in calls.items():
                v_unique = list(set(v))
                calls_unique[k] = v_unique
        return calls_unique

def dict_extend(dict_, key, values):
        if key not in dict_.keys():
                dict_[key] = values
        else:
                dict_[key].extend(values)
        return dict_

def include_extra_calls(calls: Calls, config: Config) -> Calls:
        for k, v in config['extra_calls'].items():
                if k not in calls.keys():
                        calls[k] = v
                else:
                        calls[k].extend(v)
        return calls

def find_api_callers(func: str, calls: Calls, config: Config):
        callers = [func]
        visited = [func] # loop breaker
        apis = []
        while len(callers) > 0:
                callers_new = []
                for callee in callers:
                        for k, v in calls.items():
                                # this caller does not call this callee
                                if callee not in v:
                                        continue
                                # it is part of the API
                                if k in visited:
                                        continue
                                if k in config['api'] or k in config['dead_end']:
                                        apis.append(k)
                                else:
                                        callers_new.append(k)
                                        visited.append(k)
                callers = list(set(callers_new))
                # print(callers)
                # if len(apis) > 0 and len(callers) > 0:
                #         exit(1)
        # if len(apis) == 0:
        #         print(func)
        # assert(len(apis) > 0)
        return apis

def validate(stack_usage: StackUsage, calls: Calls, config: Config, skip_threshold: int) -> None:
        all_callees = []
        for _, v in calls.items():
                all_callees.extend(v)
        all_callees = list(set(all_callees))
        dump(all_callees, 'all_callees')

        # all known functions are expected to be called at least once
        not_called = []
        for k, v in stack_usage.items():
                if k in all_callees:
                        continue
                if k in config['api']:
                        continue
                if k in config['dead_end']:
                        continue
                if k in config['white_list']:
                        continue
                if v['size'] <= skip_threshold:
                        continue
                not_called.append(k)
        # Use --dump to see the list of not called functions.
        # Investigate and either fix the call graph or add it to the white list.
        dump(not_called, 'not_called')
        assert(len(not_called) == 0)

        # all known functions are expected to be reachable from the API
        no_api_connection = {}
        for k, v in stack_usage.items():
                if k in config['api']:
                        continue
                if k in config['dead_end']:
                        continue
                if k in config['white_list']:
                        continue
                if v['size'] <= skip_threshold:
                        continue
                callers = find_api_callers(k, calls, config)
                if len(callers) == 0:
                        no_api_connection[k] = v['size']
        dump(no_api_connection, 'no_api_connection')
        assert(len(no_api_connection) == 0)

def prepare_rcalls(calls: Calls) -> Calls:
        # preparing a reverse call dictionary
        rcalls: dict[str, list] = {}
        for caller, callees in calls.items():
                for callee in callees:
                        if callee in rcalls.keys():
                                rcalls[callee].append(caller)
                        else:
                                rcalls[callee] = [caller]
        dump(rcalls, 'rcalls')
        return rcalls

def generate_call_stacks(func: str, stack_usage: StackUsage, calls: Calls, rcalls: RCalls, config: Config) -> List[CallStack]:
        call_stacks = [
                {
                        'stack': [func],
                        'size': int(stack_usage[func]['size']) if func in stack_usage.keys() else 0
                }
        ]
        # call stack generation loop
        while True:
                call_stacks_new = []
                # list of call stacks which cannot grow any more
                call_stacks_new_end = []
                for call_stack in call_stacks:
                        callee = call_stack['stack'][0]
                        if callee in config['api']:
                                call_stacks_new_end.append(call_stack)
                                continue
                        if callee not in rcalls.keys():
                                call_stacks_new_end.append(call_stack)
                                continue
                        for caller in rcalls[callee]:
                                # Note: Breaking the loop does not spoil generating
                                # other call stacks spawning from the same stem.
                                if call_stack['stack'].count(caller) == 2:
                                        continue # loop breaker
                                if caller in stack_usage.keys():
                                        caller_stack_size = int(stack_usage[caller]['size'])
                                else:
                                        caller_stack_size = 0
                                call_stacks_new.append({
                                        'stack': [caller] + call_stack['stack'],
                                        'size': call_stack['size'] + caller_stack_size
                                })
                if len(call_stacks_new) == 0:
                        break
                call_stacks = call_stacks_new + call_stacks_new_end
        return call_stacks

def call_stack_key(e):
        return e['size']

def generate_all_call_stacks(stack_usage: StackUsage, calls: Calls, rcalls: RCalls, config: Config, debug: bool = False) -> List[CallStack]:
        call_stacks = []
        # loop over called functions
        for func in rcalls.keys():
                if func in config['white_list']:
                        continue
                # if a function calls something else, call stack generation will start from its callees
                if func in calls.keys():
                        continue
                if debug:
                        print(f'Generating call stacks ending at - {func}')
                call_stacks.extend(generate_call_stacks(func, stack_usage, calls, rcalls, config))
        call_stacks.sort(reverse=True, key=call_stack_key)
        return call_stacks

def main():
        args = PARSER.parse_args()
        global DUMP
        DUMP = args.dump # pass the argument value to a global variable

        config = load_config(args.config_file)
        print('Load config - done')

        stack_usage = parse_stack_usage(args.stack_usage_stat_file, config['filter'])
        # dumping stack_usage.json to allow further processing
        dump(stack_usage, 'stack_usage', True)
        print('Stack usage - done')

        calls = parse_cflow_output(args.cflow_output_file)
        calls = include_extra_calls(calls, config)
        dump(calls, 'calls')
        print('Function calls - done')

        validate(stack_usage, calls, config, args.skip_threshold)
        print('Validation - done')

        rcalls = prepare_rcalls(calls)
        print('Reverse calls - done')

        call_stacks = generate_all_call_stacks(stack_usage, calls, rcalls, config)
        dump(call_stacks, 'call_stacks_all', True)
        print('Number of found call stacks: {}'.format(len(call_stacks)))
        print('Call stack generation - done')

if __name__ == '__main__':
        main()
