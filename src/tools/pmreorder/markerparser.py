# Copyright 2018, Intel Corporation
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


import os
import json


class MarkerParser:
    """
    Parse marker config file and command line arg provided by user
    via -x parameter.
    """
    def marker_file_parser(self, macros):
        """
        Parse markers passed by cli.
        They should be in json format:
        { "MARKER_NAME"="ENGINE_TYPE" } and separated by commas.
        """
        markers = {}
        config_file = open(macros)
        try:
            markers = json.load(config_file)
        except json.decoder.JSONDecodeError:
            print("Invalid config macros file format: ", macros,
                  "Use: {\"MARKER_NAME1\"=\"ENGINE_TYPE1\","
                  "\"MARKER_NAME2\"=\"ENGINE_TYPE2\"}")
        finally:
            config_file.close()

        return markers

    def marker_cli_parser(self, macros):
        """
        Parse markers passed by cli.
        They should be in specific format:
        MARKER_NAME=ENGINE_TYPE and separated by commas.
        """
        try:
            markers_array = macros.split(",")
            return dict(pair.split('=') for pair in markers_array)
        except ValueError:
            print("Invalid extended macros format: ", macros,
                  "Use: MARKER_NAME1=ENGINE_TYPE1,MARKER_NAME2=ENGINE_TYPE2")

    def get_markers(self, markerset):
        """
        Parse markers based on their format.
        """
        if markerset is not None:
            if os.path.exists(markerset):
                return self.marker_file_parser(markerset)
            else:
                return self.marker_cli_parser(markerset)
