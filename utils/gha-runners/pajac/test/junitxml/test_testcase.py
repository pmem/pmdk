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

from unittest import TestCase

from junitxml import result, testcase
from test.unitTestHelpers import assert_result


class TestCaseUnitTests(TestCase):
    def test_passed_testcase(self):
        assert_result(self, testcase.TestCase("test case name", "class name", 3, result.Passed()),
                                 '<testcase classname="class name" name="test case name" time="3"/>')

    def test_testcase_with_system_out(self):
        assert_result(self, testcase.TestCase("test case name", "class name", 3, result.Passed(), "1\n2\n3"),
                                 '<testcase classname="class name" name="test case name" time="3">\n'
                                  + '    <system-out>1\n2\n3</system-out>\n'
                                  + '</testcase>')

    def test_failed_testcase(self):
        assert_result(self, testcase.TestCase("test case name", "class name", 3, result.Failure("fail message", "fail type")),
                                 '<testcase classname="class name" name="test case name" time="3">\n' +
                                 '    <failure message="fail message" type="fail type"/>\n' +
                                 '</testcase>')

    def test_error_testcase(self):
        assert_result(self, testcase.TestCase("test case name", "class name", 3, result.Error("error message", "error type")),
                                 '<testcase classname="class name" name="test case name" time="3">\n' +
                                 '    <error message="error message" type="error type"/>\n' +
                                 '</testcase>')

    def test_skipped_testcase(self):
        assert_result(self, testcase.TestCase("test case name", "class name", 3,
                                                          result.Skipped("SKIP message")),
                                 '<testcase classname="class name" name="test case name" time="3">\n' +
                                 '    <skipped message="SKIP message"/>\n' +
                                 '</testcase>')
