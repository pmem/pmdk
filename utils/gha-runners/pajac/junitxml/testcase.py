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

from junitxml import xmlHelpers


class TestCase:
    def __init__(self, name: str = None, classname: str = None, time: float = None, result=None,
                 stdout: str = None):
        if name is None:
            name = ""
        if classname is None:
            classname = ""
        if time is None:
            time = 0.0

        self.name = name
        self.classname = classname
        self.time = time
        self.result = result
        self.stdout = stdout

    def to_xml(self):
        xml = xmlHelpers.create_root("testcase")
        xmlHelpers.add_attribute(xml, "name", self.name)
        xmlHelpers.add_attribute(xml, "classname", self.classname)
        xmlHelpers.add_attribute(xml, "time", self.time)

        if self.result is not None:
            xmlHelpers.add_child(xml, self.result.to_xml())

        if self.stdout is not None:
            xmlHelpers.add_child_by_name_and_value(xml, "system-out", self.stdout)
        return xml
