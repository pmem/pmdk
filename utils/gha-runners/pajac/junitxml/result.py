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


class Passed:
    def to_xml(self):
        return None


class Skipped:
    def __init__(self, message=None):
        if message is None:
            message = ""
        self.message = message

    def to_xml(self):
        xml = xmlHelpers.create_root("skipped")
        xmlHelpers.add_attribute(xml, "message", self.message)
        return xml


class Error:
    def __init__(self, message: str = None, exception_type: str = None, data: str = None):
        if message is None:
            message = ""
        if exception_type is None:
            exception_type = ""
        self.message = message
        self.exception_type = exception_type
        self.data = data

    def to_xml(self):
        xml = xmlHelpers.create_root("error")
        xmlHelpers.add_attribute(xml, "message", self.message)
        xmlHelpers.add_attribute(xml, "type", self.exception_type)
        if self.data is not None:
            xmlHelpers.add_value(xml, self.data)
        return xml


class Failure:
    def __init__(self, message: str = None, exception_type: str = None, data: str = None):
        if message is None:
            message = ""
        if exception_type is None:
            exception_type = ""
        self.message = message
        self.exception_type = exception_type
        self.data = data

    def to_xml(self):
        xml = xmlHelpers.create_root("failure")
        xmlHelpers.add_attribute(xml, "message", self.message)
        xmlHelpers.add_attribute(xml, "type", self.exception_type)
        if self.data is not None:
            xmlHelpers.add_value(xml, self.data)
        return xml
