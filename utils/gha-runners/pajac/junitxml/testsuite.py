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

from junitxml import xmlHelpers, result, testcase

test_suite_count = 0


class TestSuite:
    def __init__(self, name: str = None, test_cases: [] = None):
        if name is None:
            name = ""

        if test_cases is None:
            test_cases = []

        global test_suite_count

        # Full (class) name of the test for non-aggregated testsuite documents.
        # Class name without the package for aggregated testsuites documents. Required
        self.name = name

        # Starts at 0 for the first testsuite and is incremented by 1 for each following testsuite
        self.id = test_suite_count
        test_suite_count += 1

        self.test_cases = test_cases

        if not all(isinstance(test_case, testcase.TestCase) for test_case in self.test_cases):
            raise TypeError("Not all of items in test_cases attribute are of type test_case!")

        # Time taken (in seconds) to execute the tests in the suite
        self.time = sum(test_case.time for test_case in self.test_cases)

        # The total number of skipped tests. optional
        self.skipped = sum(isinstance(x.result, result.Skipped) for x in self.test_cases)

        # The total number of tests in the suite that failed. A failure is a test which the code has explicitly failed
        # by using the mechanisms for that purpose. e.g., via an assertEquals. optional
        self.failures = sum(isinstance(x.result, result.Failure) for x in self.test_cases)

        # The total number of tests in the suite that errored. An errored test is one that had an unanticipated problem,
        # for example an unchecked throwable; or a problem with the implementation of the test. optional
        self.errors = sum(isinstance(x.result, result.Error) for x in self.test_cases)

        # The total number of tests in the suite, required.
        self.tests = len(self.test_cases)

    def to_xml(self):
        xml = xmlHelpers.create_root("testsuite")
        xmlHelpers.add_attribute(xml, "name", self.name)
        xmlHelpers.add_attribute(xml, "errors", self.errors)
        xmlHelpers.add_attribute(xml, "failures", self.failures)
        xmlHelpers.add_attribute(xml, "id", self.id)
        xmlHelpers.add_attribute(xml, "skipped", self.skipped)
        xmlHelpers.add_attribute(xml, "time", self.time)
        xmlHelpers.add_attribute(xml, "tests", self.tests)

        for test_case in self.test_cases:
            xmlHelpers.add_child(xml, test_case.to_xml())

        return xml
