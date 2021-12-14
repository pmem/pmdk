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
from junitxml import result
from test.unitTestHelpers import assert_result


class TestResultUnitTests(unittest.TestCase):
    def test_passed(self):
        assert_result(self, result.Passed(), "", False)

    def test_failure(self):
        assert_result(self, result.Failure("failure message", "type name"), '<failure message="failure message" type="type name"/>')

    def test_failure_with_data(self):
        assert_result(self, result.Failure("failure message", "type name", "value"), '<failure message="failure message" type="type name">value</failure>')

    def test_error(self):
        assert_result(self, result.Error("error message", "type name"), '<error message="error message" type="type name"/>')

    def test_error_with_data(self):
        assert_result(self, result.Error("error message", "type name", "value\nnewline"), '<error message="error message" type="type name">value\nnewline</error>')

    def test_skipped(self):
        assert_result(self, result.Skipped("skip message"), '<skipped message="skip message"/>')