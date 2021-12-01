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

from junitxml import xmlHelpers, testsuite


class Document:
    def __init__(self, test_suites: [testsuite.TestSuite] = None):

        if test_suites is not None and len(test_suites) > 0:
            self.test_suites = test_suites
            self.tests = sum(suite.tests for suite in self.test_suites)
            self.time = sum(suite.time for suite in self.test_suites)
            self.errors = sum(suite.errors for suite in self.test_suites)
            self.failures = sum(suite.failures for suite in self.test_suites)
            self.skipped = sum(suite.skipped for suite in self.test_suites)
        else:
            self.test_suites = []
            self.tests = 0
            self.time = 0
            self.errors = 0
            self.failures = 0
            self.skipped = 0

    def to_xml(self):
        xml = xmlHelpers.create_root("testsuites")
        xmlHelpers.add_attribute(xml, "tests", self.tests)
        xmlHelpers.add_attribute(xml, "time", self.time)
        xmlHelpers.add_attribute(xml, "errors", self.errors)
        xmlHelpers.add_attribute(xml, "failures", self.failures)
        xmlHelpers.add_attribute(xml, "skipped", self.skipped)

        for suite in self.test_suites:
            xmlHelpers.add_child(xml, suite.to_xml())

        return xml
