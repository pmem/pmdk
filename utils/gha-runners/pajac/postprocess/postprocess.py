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
from typing import List
from itertools import groupby
from junitxml import result, testsuite
from postprocess.uniqueTestNameProvider import UniqueTestNameProvider


def group_skipped(testcases):
    """ Get all skipped tests, group them by skip message and return list of tuples:
        (skip_message, list_of_testcases_with_that_skip_message) """

    skipped = [s for s in testcases if isinstance(s.result, result.Skipped)]
    skipped.sort(key=lambda tc: tc.result.message)

    groups = []
    for key, group in groupby(skipped, key=lambda tc: tc.result.message):
        groups.append((key, list(group)))

    return groups


def adjust_classname(classname):
    """ Prepare proper string which will be used as a classname in Jenkins.
        We have to ensure that no dots is in the classname and remove supergroups
        created by jemalloc / main processors.
        Reason: have only PASSED / FAILED / ERRORS / SKIPPED test groups on Jenkins dashboard. """

    return classname.replace("pmdkUnitTests.", "").replace("jemallocTests.", "").replace(".", ",")


def divide_to_groups(testcases: []):
    """ Get all test cases and divide them by test result. Make result the master class of 'classname' parameter in
        order to Jenkins prints them as separate groups (testsuites would be excellent for this, but Jenkins ignores it).

        For skipped tests do one more manipulation: shift test directory to test name and set classname to skip reason.
        This is 'dirty' hack for our Jenkins to enforce grouping skipped tests by reason inside SKIPPED group."""

    skipped_groups = group_skipped(testcases)

    passed = [tc for tc in testcases if isinstance(tc.result, result.Passed)]
    failed = [tc for tc in testcases if isinstance(tc.result, result.Failure)]
    errors = [tc for tc in testcases if isinstance(tc.result, result.Error)]

    for test in passed:
        test.classname = "PASSED." + adjust_classname(test.classname)

    for test in failed:
        test.classname = "FAILED." + adjust_classname(test.classname)

    for test in errors:
        test.classname = "ERRORS." + adjust_classname(test.classname)

    for group in skipped_groups:
        prefix = "SKIPPED." + adjust_classname(group[0])
        for test in group[1]:
            temp = test.classname
            test.classname = prefix
            test.name = temp + "/" + test.name


def get_test_parameters(testcase):
    """ Pull out the test parameters from the result message in order to put them into the test name.
        This could be done only for failure messages. """

    if isinstance(testcase.result, result.Failure):
        pattern = r'^.+failed,\s+(?P<parameters>.*)'
        match = re.match(pattern, testcase.result.message)
        if match is not None:
            return match.groups()[0]

    return None


def give_unique_names(testcases: []):
    """ Iterate through all the test cases and change their names based on number of runs of particular test and its
        parameters. """

    for testcase in testcases:
        newname = UniqueTestNameProvider.provide(testcase.classname, testcase.name, get_test_parameters(testcase))
        testcase.name = newname


def postprocess(test_suites: List[testsuite.TestSuite]) -> List[testsuite.TestSuite]:
    """ Do all the stuff needed for Jenkins to display tests in a way that we want. """

    for ts in test_suites:
        give_unique_names(ts.test_cases)
        divide_to_groups(ts.test_cases)
    return test_suites
