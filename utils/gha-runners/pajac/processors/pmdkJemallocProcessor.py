#
# Copyright 2019-2020, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

import re
from junitxml import result, testcase


def adjust_classname(classname: str):
    """ Classname should be in form:
        STRING.STRING
        By default classname in this processor looks like:
        ..STRING
        so small adjustment is required."""

    return classname.replace("..", "jemallocTests.")


def process(input_string: str):
    jemalloc_pattern = r'''^===\s+  # if there are three '=' chars with following whitespace at the beginning of the line 
                        (.+?)       # This will be test group name: capture everything (but not endline)... 
                        \s+===      # ...up to whitespace following by three '=' chars
                        ([\w\W]+?)  # Capture everything (including endlines)... 
                        ^---        # ...up to line starting with three '-' characters'''

    tests_groups = re.findall(jemalloc_pattern, input_string, re.MULTILINE | re.VERBOSE)

    testcases = []
    for test_group in tests_groups:
        test_group_name = test_group[0]
        group_message = test_group[1]

        test_detail_pattern = r'''^(\w+)    # This will be test name: capture word from beginning of the line... 
                                :\s         # ...up to colon followed by whitespace
                                (\w+)       # This will be test result: capture any word'''

        tests = re.findall(test_detail_pattern, group_message, re.MULTILINE | re.VERBOSE)
        for test in tests:
            test_name = test[0]
            test_result = test[1]

            if test_result == "pass":
                r = result.Passed()
            elif test_result == "skip":
                r = result.Skipped(group_message)
            else:
                r = result.Failure(group_message, test_result)

            tc = testcase.TestCase(test_name, adjust_classname(test_group_name), 0, r)

            testcases.append(tc)

    return testcases
