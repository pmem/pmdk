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
from postprocess.uniqueTestNameProvider import UniqueTestNameProvider
import junitxml

class TestUniqueNameProvider(unittest.TestCase):
    def setUp(self):
        UniqueTestNameProvider.clear()

    def tearDown(self):
        UniqueTestNameProvider.clear()

    def test_name_provider_different_test_groups(self):
        name = "name"
        self.assertEqual(UniqueTestNameProvider.provide("group1", name), name + " (run number 0)")
        self.assertEqual(UniqueTestNameProvider.provide("group2", name), name + " (run number 0)")
        self.assertEqual(UniqueTestNameProvider.provide("group3", name), name + " (run number 0)")
        self.assertEqual(UniqueTestNameProvider.provide("group3", name), name + " (run number 1)")
        self.assertEqual(UniqueTestNameProvider.provide("group3", name), name + " (run number 2)")
        self.assertEqual(UniqueTestNameProvider.provide("group2", name), name + " (run number 1)")

    def test_name_provider_without_test_parameters(self):
        name = "name"
        group = "g"
        for i in range(0, 10):
            self.assertEqual(UniqueTestNameProvider.provide(group, name), name + " (run number " + str(i) + ")")

        different_name = "different_name"
        self.assertEqual(UniqueTestNameProvider.provide(group, different_name), different_name + " (run number 0)")
        self.assertEqual(UniqueTestNameProvider.provide(group, name), name + " (run number 10)")
        self.assertEqual(UniqueTestNameProvider.provide(group, different_name), different_name + " (run number 1)")

    def test_name_provider_with_test_parameters(self):
        name = "name"
        group = "g"
        for i in range(0, 10):
            self.assertEqual(UniqueTestNameProvider.provide(group, name, "p1"),
                             name + " (run number " + str(i) + ") with parameters: p1")

        self.assertEqual(UniqueTestNameProvider.provide(group, name, "p2"), name + " (run number 0) with parameters: p2")

        different_name = "different_name"
        self.assertEqual(UniqueTestNameProvider.provide(group, different_name, "p1"),
                         different_name + " (run number 0) with parameters: p1")
        self.assertEqual(UniqueTestNameProvider.provide(group, name), name + " (run number 0)")
        self.assertEqual(UniqueTestNameProvider.provide(group, different_name, "p1"),
                         different_name + " (run number 1) with parameters: p1")
        self.assertEqual(UniqueTestNameProvider.provide(group, different_name, "p2"),
                         different_name + " (run number 0) with parameters: p2")
