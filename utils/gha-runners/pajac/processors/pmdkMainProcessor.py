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
from collections import namedtuple

result_messages = {
    "pass": ["PASS"],
    "skip": ["SKIP"],
    "fail": ["FAILED"],
    "error": [],
    "ignored": ["START:", "all valgrind tests disabled", "failed with exit code", "DONE"]
}

testData = namedtuple('testData', ['group_name', 'case_name', 'result_string'])
lineWithNumber = namedtuple('lineWithNumber', ['number', 'line'])
failedTestInfo = namedtuple('failedTestInfo', ['group_name', 'case_name', 'related_lines'])


# helper class for handling proper result type returning:
class TestResultEntity:
    def __init__(self, test_group_name: str, test_case: str, test_result: str, additional_info: str,
                 stdout: str = None, test_parameters: str = None):
        self.test_group_name = self.adjust_classname(test_group_name)
        self.test_case = test_case
        self.test_result = test_result
        self.additional_info = additional_info
        self.stdout = stdout

    def adjust_classname(self, classname):
        """ Classname should be in form:
            STRING.STRING
            By default there is no STRING. in default classname in this processor."""
        return "pmdkUnitTests." + classname

    def get_test_case(self):
        global result_messages

        if any(self.test_result.startswith(result_message) for result_message in result_messages["pass"]):
            test_result = result.Passed()
        elif any(self.test_result.startswith(result_message) for result_message in result_messages["skip"]):
            test_result = result.Skipped(self.additional_info)
        else:
            test_result = result.Failure(self.additional_info, self.test_result)

        return testcase.TestCase(self.test_case, self.test_group_name, 0, test_result, self.stdout)

    def get_test_error(self):
        test_result = result.Error(self.additional_info, self.test_result)
        return testcase.TestCase(self.test_case, self.test_group_name, 0, test_result, self.stdout)


class MainResultsProcessor:
    def __init__(self, input_string: str):
        self.testcases = []
        self.not_completed_tests = []

        self.input_string = input_string
        self.input_lines = input_string.splitlines()

        self.process_passed_and_skipped()
        self.process_failed()
        self.process_not_completed_tests()

    def remove_last_matching_from_not_completed(self, remove_what: testData):
        # iterate backwards over not completed tests collection:
        for index in range(len(self.not_completed_tests) - 1, -1, -1):
            if remove_what.group_name == self.not_completed_tests[index].group_name \
                    and remove_what.case_name == self.not_completed_tests[index].case_name:
                del self.not_completed_tests[index]
                return

    def get_setup_pattern(self, test_group_name: str, test_case_name: str, test_parameters: str):
        """ Get pattern for regular expression for matching particular SETUP line. """

        # "test_parameters" is given in 2 formats:
        # (1) from bash framework:
        #   "TEST=short FS=non-pmem BUILD=debug RPMEM_PROVIDER=sockets RPMEM_PM=GPSPM"
        # or (2) from python framework:
        #   "short/non-pmem/debug/sockets/GPSPM"
        # "parameter_list" will contain list of values: "short", "non-pmem", "debug", "sockets", "GPSPM"
        parameter_list = []
        if '=' in test_parameters:
            for parameter in test_parameters.split():
                parameter_list.append(parameter.split('=')[1])
        elif '/' in test_parameters:
            parameter_list = test_parameters.split('/')

        # "setup_parameters_string" will contain a string in a form:
        # "(short/non-pmem/debug/sockets/GPSPM)"
        # with braces escaped with backslashes (needed for regex)
        setup_parameters_string = r'\('
        for parameter in parameter_list:
            setup_parameters_string += parameter + "/"
        setup_parameters_string = setup_parameters_string[0:-1] + r'\)'

        # below pattern should be matched with, e.g:
        # "ex_librpmem_basic/TEST1: SETUP (short/non-pmem/debug/sockets/GPSPM)"
        return r'^{test_group_name}/+{test_case_name}:\s+SETUP\s+{setup_parameters_string}' \
            .format(test_group_name=test_group_name, test_case_name=test_case_name,
                    setup_parameters_string=setup_parameters_string)

    def get_test_trace_from_pair_setup_fail(self, fail_message: str, test_group_name: str, test_case_name: str,
                                            test_parameters: str):
        """ Get test trace with help of detailed parameters, printed when SETUP / FAIL stage is done.
            Unfortunately not all tests prints those parameters, but there is more: some tests prints different SETUP
            parameters then FAIL parameters. In this cases this function will return None. """

        if test_parameters == "":
            return None

        test_setup_pattern = self.get_setup_pattern(test_group_name, test_case_name, test_parameters)

        starting_index = None
        ending_index = None

        # find all indexes of lines related with test+params.
        # If not found TEST SETUP and / or TEST FAIL, return None - unable to pull single instance from the logs.
        # We should print generic test trace, based on test mentioning.
        for index in range(0, len(self.input_lines)):
            if starting_index is None and re.match(test_setup_pattern, self.input_lines[index], re.MULTILINE):
                starting_index = index

            if starting_index is not None and self.input_lines[index] == fail_message:
                ending_index = index
                break

        if starting_index is not None and ending_index is not None:

            trace = "--- [approach: detailed parameters for SETUP + FAIL] test trace for: " + test_group_name + "/" + test_case_name + " " + test_parameters + " ---\n"

            for index in range(starting_index, ending_index + 1):
                trace += "[line " + str(index + 1) + "] " + self.input_lines[index] + "\n"

            return trace

        else:
            return None

    def get_test_trace_nearest(self, fail_message: str, test_group_name: str, test_case_name: str,
                               test_parameters: str):
        """ Get test trace by searching nearest (backward from FAIL) SETUP with matching test group and case name.
            If this cannot be done (e. g. there is no matching SETUP step), return None. """

        ending_index = self.input_lines.index(fail_message)
        setup_pattern = "^{test_group_name}/+{test_case_name}: SETUP.*$".format(test_group_name=test_group_name,
                                                                           test_case_name=test_case_name)

        starting_index = None

        # range monotonically decreasing, from index where fail message was found, down to 0:
        range_lower_boundary_exclusive = -1
        range_step = -1
        for index in range(ending_index, range_lower_boundary_exclusive, range_step):
            if re.match(setup_pattern, self.input_lines[index], re.MULTILINE):
                starting_index = index
                break

        if starting_index is None:
            return None

        trace = "--- [approach: nearest matching SETUP] test trace for: " + test_group_name + "/" + test_case_name + " " + test_parameters + " ---\n"

        for index in range(starting_index, ending_index + 1):
            if index == ending_index:
                trace += "[==line " + str(index + 1) + "==] "
            else:
                trace += "[line " + str(index + 1) + "] "
            trace += self.input_lines[index] + "\n"

        return trace

    def get_test_trace_generic(self, fail_message: str, test_group_name: str, test_case_name: str, test_parameters: str = None):
        """ Get test trace by searching ALL instances of 'test_group/test_case' from beginning up to found failure
            message. This could producing really big result xml when input log is big and between runs of the same tests
            there are lots of different tests. """

        global lineWithNumber

        if fail_message is not None:
            ending_index = self.input_lines.index(fail_message)
        else:
            ending_index = len(self.input_lines)

        full_test_name = test_group_name + "/+" + test_case_name

        test_name_pattern = "^.*" + full_test_name + ".*$"

        this_test_lines_with_numbers = [lineWithNumber(index, value)
                                        for index, value
                                        in enumerate(self.input_lines[0:ending_index + 1])
                                        if re.match(test_name_pattern, value)]

        first_related_line_number = this_test_lines_with_numbers[0].number
        last_related_line_number = this_test_lines_with_numbers[-1].number

        all_related_data = "--- [approach: all mentions from beginning] test trace for: " + full_test_name + " ---\n"
        for line in this_test_lines_with_numbers:
            all_related_data += "[line " + str(line.number + 1) + "] " + line.line + "\n"

        all_related_data += "\n\n--- all console output from SETUP up to last trace ---\n" + \
                            '\n'.join(self.input_lines[first_related_line_number:last_related_line_number + 1])

        return all_related_data, this_test_lines_with_numbers

    def process_passed_and_skipped(self):
        pattern = r'''          # pattern should match every line starting with...
                    (\w+?)      # a word which is first captured group - test group name
                    /+          # then immediately should be forward slash '/' character (one or more)
                    (\w+?)      # then will be word - second captured group, test name 
                    \s*:\s      # then should be colon (separated or not with space(s))
                    (.+)        # last captured group will be test result'''

        for line_index in range(0, len(self.input_lines)):
            test_match = re.match(pattern, self.input_lines[line_index], re.MULTILINE | re.VERBOSE)

            if test_match is None:
                continue

            test_match = test_match.groups()

            test_group_name = test_match[0]
            test_case_name = test_match[1]
            test_result_string = test_match[2]

            global result_messages
            global testData
            this_phase_results = result_messages["pass"] + result_messages["skip"]
            this_phase_results_ignored = result_messages["fail"] + result_messages["error"] + result_messages["ignored"]

            # get nice trace of test and put it in 'stdout' (line with result and some context lines)
            context_lines = 5
            min_index = max(line_index - context_lines, 0)
            max_index = line_index
            stdout = ""
            for i in range(min_index, max_index + 1):
                if i == line_index:
                    prefix = "[==line " + str(i + 1) + "==] "
                else:
                    prefix = "[line " + str(i + 1) + "] "
                stdout += prefix + self.input_lines[i] + "\n"
            # remove last \n character:
            if len(stdout) > 1:
                stdout = stdout[0:-1]

            if any(test_result_string.startswith(valid_result) for valid_result in this_phase_results):
                # if found passed or skipper - add it to parsed testcases and remove SETUP entry from not completed list:
                test_result = TestResultEntity(test_group_name, test_case_name, test_result_string,
                                               test_result_string, stdout)
                self.testcases.append(test_result.get_test_case())
                self.remove_last_matching_from_not_completed(testData(test_group_name, test_case_name, test_result_string))

            elif not any(test_result_string.startswith(ignored_result) for ignored_result in this_phase_results_ignored):
                # if not passed or skipped, and not any of the ignored - but matches pattern, keep data of this test because maybe there will be some
                # not completed test, e. g.: when setup was found and 'pass' was not found, etc.:
                self.not_completed_tests.append(testData(test_group_name, test_case_name, test_result_string))

    def process_failed(self):
        fail_pattern_from_bash_framework = r'''
                (?P<fail_message>^           # pattern should match every line (which will be 0th captured group) starting with...
                RUNTESTS:\s                  # "RUNTESTS:" string following with a whitespace
                stopping:\s+                 # then "stopping:" string following with whitespace or more
                (?P<test_group>\w+)          # first captured group will be the name of the test group...
                /+                           # followed immediately with a forward slash character (one or more)
                (?P<test_name>\w+)           # second captured group will be the name of the test
                \s+(?:failed|timed\sout),\s+ # then should appear "failed," or "timed out," string surrounded with whitespaces
                (?P<parameters>.*)           # the rest of the line will be third captured group: test parameters
                )                            # ...and enclosing brace for the 0th capture group'''

        fail_pattern_from_python_framework = r'''
                (?P<fail_message>^           # pattern should match every line (which will be 0th captured group)
                (?P<test_group>\w+)          # first captured group will be the name of the test group...
                /+                           # followed immediately with a forward slash character (one or more)
                (?P<test_name>\w+):          # second captured group will be the name of the test followed by colon
                \s+(?:FAILED)\s+             # then should appear "FAILED" string surrounded with whitespaces
                (?P<parameters>.*)           # the rest of the line will be third captured group: test parameters
                )                            # ...and enclosing brace for the 0th capture group'''

        fail_pattern_from_windows_framework = r'''
                (?P<fail_message>^           # pattern should match every line (which will be 0th captured group)
                (?P<test_group>\w+)          # first captured group will be the name of the test group...
                /+                           # followed immediately with a forward slash character (one or more)
                (?P<test_name>\w+):          # second captured group will be the name of the test followed by colon
                \s+(?:FAILED)                # then should appear "FAILED" string, preceded with whitespaces
                (?P<parameters>)             # "parameters" capture group has to be empty - there is no parameters given in the Windows failure print
                \S.*                         # then immediately appears non-whitespace character followed by some string 
                )                            # ...and enclosing brace for the 0th capture group'''

        # find all tests that are failed:
        all_failed = re.findall(fail_pattern_from_bash_framework, self.input_string, re.MULTILINE | re.VERBOSE) + \
                     re.findall(fail_pattern_from_python_framework, self.input_string, re.MULTILINE | re.VERBOSE) + \
                     re.findall(fail_pattern_from_windows_framework, self.input_string, re.MULTILINE | re.VERBOSE)

        # find all lines related with given failed tests:
        for failed_test in all_failed:
            fail_message = failed_test[0]
            failed_test_group = failed_test[1]
            failed_test_name = failed_test[2]
            failed_test_parameters = failed_test[3]

            # try to get test trace with cascade: from the most detailed approach up to most general:
            test_trace_string = self.get_test_trace_from_pair_setup_fail(fail_message, failed_test_group,
                                                                         failed_test_name, failed_test_parameters)

            if test_trace_string is None:
                test_trace_string = self.get_test_trace_nearest(fail_message, failed_test_group,
                                                                failed_test_name, failed_test_parameters)

                if test_trace_string is None:
                    test_trace_string, ignored = self.get_test_trace_generic(fail_message, failed_test_group,
                                                                             failed_test_name)

            test_result = TestResultEntity(failed_test_group, failed_test_name,
                                           "", fail_message, test_trace_string, failed_test_parameters)

            self.testcases.append(test_result.get_test_case())
            self.remove_last_matching_from_not_completed(testData(failed_test_group, failed_test_name, ""))

    def process_not_completed_tests(self):
        global testData
        for not_completed_test in self.not_completed_tests:
            test_trace_message = self.get_test_trace_generic(None, not_completed_test.group_name,
                                                             not_completed_test.case_name)

            r = TestResultEntity(not_completed_test.group_name, not_completed_test.case_name,
                                 "PASS/SKIP/FAILURE for this test not found!", not_completed_test.result_string,
                                 test_trace_message[0])

            self.testcases.append(r.get_test_error())
        self.not_completed_tests.clear()


def process(input_string: str):
    return MainResultsProcessor(input_string).testcases
