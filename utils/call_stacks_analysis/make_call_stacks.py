#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023-2024, Intel Corporation

import argparse
import json
import re

from typing import List, Dict, Any

# https://peps.python.org/pep-0589/ requires Python >= 3.8
# from typing import TypedDict
# https://peps.python.org/pep-0613/ requires Python >= 3.10
# from typing import TypeAlias

# Max empirically measured call stacks' size for ndctl_* calls (v78)
NDCTL_CALL_STACK_ESTIMATE = 4608

# Max empirically measured call stacks' size for daxctl_* calls (v78)
DAXCTL_CALL_STACK_ESTIMATE = 7344

PARSER = argparse.ArgumentParser()
PARSER.add_argument('-u', '--stack-usage-file', default='stack_usage.txt')
PARSER.add_argument('-f', '--cflow-output-file', default='cflow.txt')
PARSER.add_argument('-e', '--extra-calls', default='extra_calls.json')
PARSER.add_argument('-a', '--api-file', default='api.txt')
PARSER.add_argument('-w', '--white-list', default='white_list.json')
PARSER.add_argument('-d', '--dump', action='store_true', help='Dump debug files')
PARSER.add_argument('-s', '--dump-all-stacks', action='store_true',
                    help='Dump call_stacks_all.json files. Should be used with caution because the file can be >2GB and dumping takes a significant amount of time.')
PARSER.add_argument('-l', '--filter-lower-limit', type=int, default=0,
        help='Include only call stacks of stack usage above the limit.')
PARSER.add_argument('-i', '--filter-api-file',
        help='Include only call stacks starting from API calls enumerated in the provided file.')
PARSER.add_argument('-t', '--skip-threshold', type=int, default=0,
        help='Ignore non-reachable function if its stack usage <= threshold')

# API: TypeAlias = List[str] # for Python >= 3.10
API = List[str] # for Python < 3.8

# class WhiteList(TypedDict): # for Python >= 3.8
#     not_called: List[str]
WhiteList = Dict[str, List[str]] # for Python < 3.8

# APIFilter: TypeAlias = List[str] # for Python >= 3.10
APIFilter = List[str] # for Python < 3.8

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

def load_from_json(file_name: str) -> Any:
        with open(file_name, 'r') as file:
                return json.load(file)

def txt_filter(line: str) -> bool:
        if line == '\n': # drop empty lines
                return False
        if line[0] == '#': # drop comment lines
                return False
        return True

def load_from_txt(file_name: str) -> List[str]:
        with open(file_name, 'r') as file:
                ret = file.readlines()
        ret = list(filter(txt_filter, ret))
        # remove end-line characters
        return [line[:-1] for line in ret]

def parse_stack_usage(stack_usage_file: str) -> StackUsage:
        funcs = {}
        with open(stack_usage_file, 'r') as file:
                for line in file:
                        # 8432 out_common : src/nondebug/libpmem/out.su:out.c dynamic,bounded
                        # 336 __iomem_get_dev_resource : ./build/daxctl/lib/libdaxctl.so.1.2.5.p/.._.._util_iomem.c.su:iomem.c dynamic,bounded
                        # 112 ctl_find_node.isra.5 : src/nondebug/common/ctl.su:ctl.c static
                        found = re.search('([0-9]+) ([a-zA-Z0-9_]+)(.[a-zA-Z0-9.]+)* : ([a-zA-Z0-9.:/_-]+) ([a-z,]+)', line)
                        if found:
                                func = found.group(2)
                                size = int(found.group(1))
                                if func in funcs.keys():
                                        curr_size = funcs[func]['size']
                                        if size != curr_size:
                                                print(f'Warning: Incompatible function records [{func}].size: {size} != {curr_size}. Continue with the biggest reported size.')
                                                if curr_size > size:
                                                        size = curr_size
                                funcs[func] = {'size': size, 'type': found.group(5)}
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

def include_extra_calls(calls: Calls, extra_calls: Calls) -> Calls:
        for k, v in extra_calls.items():
                if k not in calls.keys():
                        calls[k] = v
                else:
                        calls[k].extend(v)
        return calls

def find_api_callers(func: str, calls: Calls, api: API):
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
                                if k in api:
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

def validate(stack_usage: StackUsage, calls:Calls, api: API, white_list: WhiteList, skip_threshold: int) -> None:
        all_callees = [v for _, vs in calls.items()
                       for v in vs]
        all_callees = list(set(all_callees))
        dump(all_callees, 'all_callees')

        # all known functions are expected to be called at least once
        not_called = []
        for k, v in stack_usage.items():
                if k in all_callees:
                        continue
                if k in api:
                        continue
                if k in white_list['not_called']:
                        continue
                if v['size'] <= skip_threshold:
                        continue
                not_called.append(k)
        # Use --dump to see the list of not called functions.
        # Investigate and either fix the call graph or add it to the white list.
        file_name = 'not_called'
        if len(not_called) > 0:
                dump(not_called, file_name, True)
                raise Exception("There are some unreachable functions. See the {}.json file.".format(file_name))
        dump(not_called, file_name)

        # all known functions are expected to be reachable from the API
        no_api_connection = {}
        for k, v in stack_usage.items():
                if k in api:
                        continue
                if k in white_list['not_called']:
                        continue
                if v['size'] <= skip_threshold:
                        continue
                callers = find_api_callers(k, calls, api)
                if len(callers) == 0:
                        no_api_connection[k] = v['size']
        file_name = 'no_api_connection'
        if len(no_api_connection) > 0:
                dump(no_api_connection, file_name, True)
                raise Exception("There are some functions unreachable from API. See the {}.json file.".format(file_name))
        dump(no_api_connection, file_name)

        # dump all zero-sized functions for further assesment
        all_callers = [k for k, _ in calls.items()]
        all_funcs = list(set(all_callees + all_callers))
        zero_funcs = [func for func in all_funcs if func not in stack_usage.keys()]
        zero_funcs = list(filter(lambda f: len(f) > 0, zero_funcs))
        zero_funcs_grep = [func for func in zero_funcs]
        dump(zero_funcs, 'zero_funcs')
        global DUMP
        if DUMP:
                with open('zero_funcs.txt', 'w') as outfile:
                        outfile.write('\n'.join(zero_funcs_grep))

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

def call_stacks_find_the_biggest_starting_at(call_stacks: List[CallStack], api_func: str):
        call_stack_max = {
                'size': 0
        }
        for call_stack in call_stacks:
                if api_func == call_stack['stack'][0]:
                        if call_stack['size'] > call_stack_max['size']:
                                call_stack_max = call_stack
        if 'stack' not in call_stack_max.keys():
                raise Exception(f'No call stack starting at {api_func} has been found.')
        return call_stack_max

def generate_call_stacks_calling_api(call_stack_max: CallStack, callers: List[str], call_stacks: List[CallStack]) -> List[CallStack]:
        call_stacks_new = []
        for call_stack in call_stacks:
                # If the caller is the last position in the stack it means
                # the stack can continue through the API call.
                if call_stack['stack'][-1] in callers:
                        new_call_stack = {
                                'stack': call_stack['stack'] + call_stack_max['stack'],
                                'size': call_stack['size'] + call_stack_max['size']
                        }
                        call_stacks_new.append(new_call_stack)
        return call_stacks_new

def generate_call_stacks_calling_apis(apis_called_internally: List[str], rcalls: RCalls, call_stacks: List[CallStack], debug: bool):
        # list of call stacks which can grow
        call_stacks_called: List[CallStack] = [
                call_stacks_find_the_biggest_starting_at(call_stacks, api)
                        for api in apis_called_internally
        ]
        # list of call stacks which cannot grow any more
        call_stacks_new_end = []
        while len(call_stacks_called) > 0:
                call_stacks_new = []
                for call_stack in call_stacks_called:
                        api_func = call_stack['stack'][0]
                        callers = rcalls[api_func]
                        call_stacks_generated = generate_call_stacks_calling_api(call_stack, callers, call_stacks)
                        call_stacks_new.extend(
                                [cs for cs in call_stacks_generated
                                 if cs['stack'][0] in apis_called_internally])
                        call_stacks_new_end.extend(
                                [cs for cs in call_stacks_generated
                                 if cs['stack'][0] not in apis_called_internally])
                # reduce - keep only the max stack for each API call
                apis = list(set([cs['stack'][0] for cs in call_stacks_new]))
                call_stacks_new_reduced = []
                for api in apis:
                        cs_max = call_stacks_find_the_biggest_starting_at(call_stacks_new, api)
                        call_stacks_new_reduced.append(cs_max)
                if debug:
                        print('end: {}, new: {}, reduced: {}'.format(
                                len(call_stacks_new_end),
                                len(call_stacks_new),
                                len(call_stacks_new_reduced)))
                call_stacks_called = call_stacks_new_reduced
        return call_stacks_new_end

def generate_call_stacks_basic(func: str, stack_usage: StackUsage, rcalls: RCalls, api: API) -> List[CallStack]:
        size = 0
        if func.find("ndctl_", 0) == 0:
                size = NDCTL_CALL_STACK_ESTIMATE
        elif func.find("daxctl_", 0) == 0:
                size = DAXCTL_CALL_STACK_ESTIMATE
        elif func in stack_usage.keys():
                size = stack_usage[func]['size']

        call_stacks = [
                {
                        'stack': [func],
                        'size': size
                }
        ]
        # list of call stacks which cannot grow any more
        call_stacks_completed = []
        # call stack generation loop
        while len(call_stacks) > 0:
                call_stacks_in_progress = []
                for call_stack in call_stacks:
                        callee = call_stack['stack'][0]
                        if callee in api:
                                call_stacks_completed.append(call_stack)
                                continue
                        if callee not in rcalls.keys():
                                call_stacks_completed.append(call_stack)
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
                                call_stacks_in_progress.append({
                                        'stack': [caller] + call_stack['stack'],
                                        'size': call_stack['size'] + caller_stack_size
                                })
                call_stacks = call_stacks_in_progress
        return call_stacks_completed

def call_stack_key(e):
        return e['size']

def generate_call_stacks(stack_usage: StackUsage, calls: Calls, rcalls: RCalls, api: API, debug: bool = False) -> List[CallStack]:
        # Find all APIs called internally
        apis_called_internally = []
        for api_func in api:
                if api_func in rcalls.keys():
                        apis_called_internally.append(api_func)
        dump(apis_called_internally, 'apis_called_internally')

        internal_api_callers = []
        for api_func in apis_called_internally:
                internal_api_callers.extend(rcalls[api_func])
        internal_api_callers = list(set(internal_api_callers))
        dump(internal_api_callers, 'internal_api_callers')

        if debug:
                print('Basic call stack generation')

        # loop over called functions
        call_stacks = []
        for func in rcalls.keys():
                # If a function calls something else, call stack generation will
                # start from its callees unless it is an internal API caller and
                # does not call non-API functions. For simplicity, it is assumed
                # an internal API caller can start a call stack no matter if it
                # also calls non-API callees.
                if func in calls.keys() and func not in internal_api_callers:
                        continue
                if debug:
                        print(f'Generating call stacks ending at - {func}')
                # Note: generate_call_stacks stop further processing of a call
                # stacks when an API function is found. API functions called
                # internally are handled below.
                call_stacks.extend(generate_call_stacks_basic(func, stack_usage, rcalls, api))
        dump(call_stacks, 'call_stacks_basic')

        if debug:
                print('Call stacks calling APIs generation')

        call_stacks_calling_apis = generate_call_stacks_calling_apis(apis_called_internally, rcalls, call_stacks, debug)
        if debug:
                call_stacks_calling_apis.sort(reverse=True, key=call_stack_key)
        dump(call_stacks_calling_apis, 'call_stacks_calling_apis')

        call_stacks.extend(call_stacks_calling_apis)
        call_stacks.sort(reverse=True, key=call_stack_key)
        return call_stacks

def filter_call_stacks(call_stacks: List[CallStack], api_filter_file: APIFilter) -> List[CallStack]:
        api_filter = load_from_txt(api_filter_file)
        dump(api_filter, 'api_filter')
        print('Load API Filter - done')

        call_stacks_filtered = []
        for call_stack in call_stacks:
                if call_stack['stack'][0] in api_filter:
                        call_stacks_filtered.append(call_stack)
        return call_stacks_filtered

def main():
        args = PARSER.parse_args()
        global DUMP
        DUMP = args.dump # pass the argument value to a global variable

        extra_calls = load_from_json(args.extra_calls)
        print('Load extra calls - done')

        api = load_from_txt(args.api_file)
        dump(api, 'api')
        print('Load API - done')

        white_list = load_from_json(args.white_list)
        print('White List - done')

        stack_usage = parse_stack_usage(args.stack_usage_file)
        # dumping stack_usage.json to allow further processing
        dump(stack_usage, 'stack_usage', True)
        print('Stack usage - done')

        calls = parse_cflow_output(args.cflow_output_file)
        calls = include_extra_calls(calls, extra_calls)
        dump(calls, 'calls')
        print('Function calls - done')

        validate(stack_usage, calls, api, white_list, args.skip_threshold)
        print('Validation - done')

        rcalls = prepare_rcalls(calls)
        print('Reverse calls - done')

        call_stacks = generate_call_stacks(stack_usage, calls, rcalls, api, DUMP)
        dump(call_stacks, 'call_stacks_all', args.dump_all_stacks)
        print('Generate call stacks - done')
        print('Number of call stacks: {}'.format(len(call_stacks)))

        if args.filter_lower_limit > 0:
                def above_limit(call_stack: CallStack) -> bool:
                        return call_stack['size'] > args.filter_lower_limit
                too_big = [call_stack
                           for call_stack in call_stacks
                                if above_limit(call_stack)]
                call_stacks = too_big
                print('Filter out call stacks <= {} - done'.format(args.filter_lower_limit))
                print('Number of call stacks: {}'.format(len(call_stacks)))

        if args.filter_api_file is not None:
                call_stacks = filter_call_stacks(call_stacks, args.filter_api_file)
                print('Filter our call stacks not in the API filter - done')
                print('Number of call stacks: {}'.format(len(call_stacks)))

        dump(call_stacks, 'call_stacks_filtered', True)

if __name__ == '__main__':
        main()
