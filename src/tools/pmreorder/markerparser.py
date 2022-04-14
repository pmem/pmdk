# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2022, Intel Corporation


import os
import json


class MarkerParser:
    """
    Parse marker config file and command line arg provided by user
    via -x parameter.
    """

    def marker_file_parser(self, macros):
        """
        Parse markers passed by file.
        They should be in json format:
        { "MARKER_NAME"="ENGINE_TYPE" } and separated by commas.
        """
        markers = {}
        try:
            with open(macros) as config_file:
                markers = json.load(config_file)
        except json.decoder.JSONDecodeError:
            print(
                "Invalid config macros file format: ",
                macros,
                'Use: {"MARKER_NAME1"="ENGINE_TYPE1",'
                '"MARKER_NAME2"="ENGINE_TYPE2"}',
            )

        return markers

    def marker_cli_parser(self, macros):
        """
        Parse markers passed by cli.
        They should be in specific format:
        MARKER_NAME=ENGINE_TYPE and separated by commas.
        """
        try:
            markers_array = macros.split(",")
            return dict(pair.split("=") for pair in markers_array)
        except ValueError:
            print(
                "Invalid extended macros format: ",
                macros,
                "Use: MARKER_NAME1=ENGINE_TYPE1,MARKER_NAME2=ENGINE_TYPE2",
            )

    def get_markers(self, markerset):
        """
        Parse markers based on their format.
        """
        if markerset is not None:
            if os.path.exists(markerset):
                return self.marker_file_parser(markerset)
            else:
                return self.marker_cli_parser(markerset)
