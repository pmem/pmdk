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

from postprocess.postprocess import postprocess
from preprocess.preprocess import preprocess
from processors import pmdkJemallocProcessor, pmdkMainProcessor
from junitxml import xmlHelpers, testsuite, document
from typing import List


def to_test_suites(input_string: str) -> List[testsuite.TestSuite]:
    test_suites = []

    jemallocs = pmdkJemallocProcessor.process(input_string)
    if len(jemallocs) > 0:
        test_suites.append(testsuite.TestSuite("PMDK jemalloc tests", jemallocs))

    pmdk_unit_tests = pmdkMainProcessor.process(input_string)
    if len(pmdk_unit_tests) > 0:
        test_suites.append(testsuite.TestSuite("PMDK unit tests", pmdk_unit_tests))

    return test_suites


def from_test_suites_to_xml(test_suites: testsuite.TestSuite) -> str:
    xml = document.Document(test_suites).to_xml()
    return xmlHelpers.to_pretty_string(xml)


def process(input_string: str) -> str:
    preprocessed_input_string = preprocess(input_string)
    test_suites = to_test_suites(preprocessed_input_string)
    postprocessed_test_suites = postprocess(test_suites)
    xml_content = from_test_suites_to_xml(postprocessed_test_suites)
    return xml_content