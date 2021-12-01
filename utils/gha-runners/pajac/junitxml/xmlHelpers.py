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

import xml.etree.ElementTree as et
from xml.dom import minidom


def escape_xml(xml_string: str):
    s = xml_string.replace("&", "&amp;")
    s = s.replace("<", "&lt;")
    s = s.replace(">", "&gt;")
    s = s.replace("\"", "&quot;")
    return s


def create_root(name: str) -> et.Element:
    return et.Element(escape_xml(name))


def add_attribute(xml: et.Element, name: str, value):
    xml.set(escape_xml(name), escape_xml(str(value)))


def add_child(xml: et.Element, child: et.Element):
    if child is not None:
        xml.append(child)


def add_value(xml: et.Element, value):
    xml.text = escape_xml(str(value))


def add_child_by_name_and_value(xml: et.Element, name: str, value) -> et.Element:
    child = et.Element(escape_xml(name))
    child.text = str(escape_xml(value))
    add_child(xml, child)
    return child


def add_child_by_name(xml: et.Element, name: str) -> et.Element:
    child = et.Element(name)  # not escaping due to escaping in 'add_child' function
    add_child(xml, child)
    return child


def to_string(xml: et.Element) -> str:
    if xml is not None:
        return et.tostring(xml).decode()
    else:
        return ""


def to_pretty_string(xml: et.Element) -> str:
    if xml is not None:
        return minidom.parseString(to_string(xml)).toprettyxml(indent="    ")
    else:
        return ""


def from_string(string: str) -> et.Element:
    return et.fromstring(string)


