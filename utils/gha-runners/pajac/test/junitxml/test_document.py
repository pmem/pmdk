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

import unittest

from junitxml import document, testsuite, result, testcase
from test.unitTestHelpers import assert_result


class TestDocument(unittest.TestCase):
    def setUp(self):
        self.temp = testsuite.test_suite_count
        testsuite.test_suite_count = 0

        testSuite0 = testsuite.TestSuite("TEST SUITE 0", [
            testcase.TestCase("test 1", "class 1", 10, result.Passed()),
            testcase.TestCase("test 2", "class 2", 20, result.Skipped("SKIPPED MESSAGE"))
        ])
        self.document0 = document.Document([testSuite0])

        testSuite1 = testsuite.TestSuite("TEST SUITE 1", [])
        self.document1 = document.Document([testSuite1])

        testSuite2 = testsuite.TestSuite("TEST SUITE 2", [
            testcase.TestCase("test 1", "class 1", 10, result.Passed()),
            testcase.TestCase("test 2", "class 2", 20, result.Skipped("SKIPPED MESSAGE")),
            testcase.TestCase("test 3", "class 3", 30, result.Error("ERROR MESSAGE", "TYPE MESSAGE")),
            testcase.TestCase("test 4", "class 4", 40, result.Error("ERROR MESSAGE", "TYPE MESSAGE")),
            testcase.TestCase("test 5", "class 5", 50.5, result.Failure("FAIL MESSAGE", "TYPE MESSAGE")),
        ])
        self.document2 = document.Document([testSuite0, testSuite1, testSuite2])

        self.document3 = document.Document()



    def tearDown(self):
        testsuite.test_suite_count = self.temp

    def test_suite_0_ctor(self):
        self.assertEqual(self.document0.errors, 0)
        self.assertEqual(self.document0.failures, 0)
        self.assertEqual(self.document0.skipped, 1)
        self.assertEqual(self.document0.tests, 2)
        self.assertEqual(self.document0.time, 30)
        self.assertEqual(len(self.document1.test_suites), 1)

    def test_suite_0_xml(self):
        expectedXml = R"""<testsuites errors="0" failures="0" skipped="1" tests="2" time="30">
    <testsuite errors="0" failures="0" id="0" name="TEST SUITE 0" skipped="1" tests="2" time="30">
        <testcase classname="class 1" name="test 1" time="10"/>
        <testcase classname="class 2" name="test 2" time="20">
            <skipped message="SKIPPED MESSAGE"/>
        </testcase>
    </testsuite>
</testsuites>"""
        assert_result(self, self.document0, expectedXml)

    # set 1


    def test_suite_1_ctor(self):
        self.assertEqual(self.document1.errors, 0)
        self.assertEqual(self.document1.failures, 0)
        self.assertEqual(self.document1.skipped, 0)
        self.assertEqual(self.document1.tests, 0)
        self.assertEqual(self.document1.time, 0)
        self.assertEqual(len(self.document1.test_suites), 1)

    def test_suite_1_xml(self):
        expectedXml = '<testsuites errors="0" failures="0" skipped="0" tests="0" time="0">\n' \
                      '    <testsuite errors="0" failures="0" id="1" name="TEST SUITE 1" skipped="0" tests="0" time="0"/>\n' \
                      '</testsuites>'
        assert_result(self, self.document1, expectedXml)

    def test_suite_2_ctor(self):
        self.assertEqual(self.document2.errors, 2)
        self.assertEqual(self.document2.failures, 1)
        self.assertEqual(self.document2.skipped, 1 + 1)
        self.assertEqual(self.document2.tests, 5 + 2)
        self.assertEqual(self.document2.time, 150.5 + 30)
        self.assertEqual(len(self.document2.test_suites), 3)

    def test_suite_2_xml(self):
        expectedXml = R"""<testsuites errors="2" failures="1" skipped="2" tests="7" time="180.5">
    <testsuite errors="0" failures="0" id="0" name="TEST SUITE 0" skipped="1" tests="2" time="30">
        <testcase classname="class 1" name="test 1" time="10"/>
        <testcase classname="class 2" name="test 2" time="20">
            <skipped message="SKIPPED MESSAGE"/>
        </testcase>
    </testsuite>
    <testsuite errors="0" failures="0" id="1" name="TEST SUITE 1" skipped="0" tests="0" time="0"/>
    <testsuite errors="2" failures="1" id="2" name="TEST SUITE 2" skipped="1" tests="5" time="150.5">
        <testcase classname="class 1" name="test 1" time="10"/>
        <testcase classname="class 2" name="test 2" time="20">
            <skipped message="SKIPPED MESSAGE"/>
        </testcase>
        <testcase classname="class 3" name="test 3" time="30">
            <error message="ERROR MESSAGE" type="TYPE MESSAGE"/>
        </testcase>
        <testcase classname="class 4" name="test 4" time="40">
            <error message="ERROR MESSAGE" type="TYPE MESSAGE"/>
        </testcase>
        <testcase classname="class 5" name="test 5" time="50.5">
            <failure message="FAIL MESSAGE" type="TYPE MESSAGE"/>
        </testcase>
    </testsuite>
</testsuites>"""
        assert_result(self, self.document2, expectedXml)

    def test_suite_3_ctor(self):
        self.assertEqual(self.document3.errors, 0)
        self.assertEqual(self.document3.failures, 0)
        self.assertEqual(self.document3.skipped, 0)
        self.assertEqual(self.document3.tests, 0)
        self.assertEqual(self.document3.time, 0)
        self.assertEqual(len(self.document3.test_suites), 0)

    def test_suite_3_xml(self):
        expectedXml = '<testsuites errors="0" failures="0" skipped="0" tests="0" time="0"/>'
        assert_result(self, self.document3, expectedXml)