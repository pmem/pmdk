#
# Copyright 2018-2019, Intel Corporation
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

import argparse
import statemachine
import opscontext
import consistencycheckwrap
import loggingfacility
import markerparser
import sys
import reorderengines


def main():
    pmreorder_version = "unknown"

    '''
    Argv[1] should be given in order to use -v or --version flag. Argv[1]
    is passed from the installed script. We must check whether argv[1]
    was given since it is always a version of pmreorder.
    '''
    if len(sys.argv) > 1 and sys.argv[1][0] != "-":
        pmreorder_version = sys.argv[1]

    # TODO unicode support
    # TODO parameterize reorder engine type
    parser = argparse.ArgumentParser(description="Store reordering tool")
    parser.add_argument("-l", "--logfile",
                        required=True,
                        help="the pmemcheck log file to process")
    parser.add_argument("-c", "--checker",
                        choices=consistencycheckwrap.checkers,
                        default=consistencycheckwrap.checkers[0],
                        help="choose consistency checker type")
    parser.add_argument("-p", "--path",
                        required=True,
                        help="path to the consistency checker and arguments",
                        nargs='+')
    parser.add_argument("-n", "--name",
                        help="consistency check function " +
                        "for the 'lib' checker")
    parser.add_argument("-o", "--output",
                        help="set the logger output file")
    parser.add_argument("-e", "--output-level",
                        choices=loggingfacility.log_levels,
                        help="set the output log level")
    parser.add_argument("-x", "--extended-macros",
                        help="list of pairs MARKER=ENGINE or " +
                        "json config file")
    parser.add_argument("-v", "--version",
                        help="print version of the pmreorder",
                        action="version",
                        version="%(prog)s (" + pmreorder_version + ")")
    engines_keys = list(reorderengines.engines.keys())
    parser.add_argument("-r", "--default-engine",
                        help="set default reorder engine " +
                        "default=NoReorderNoChecker",
                        choices=engines_keys,
                        default=engines_keys[0])
    args = parser.parse_args()
    logger = loggingfacility.get_logger(
                                        args.output,
                                        args.output_level)
    checker = consistencycheckwrap.get_checker(
                                               args.checker,
                                               ' '.join(args.path),
                                               args.name)

    markers = markerparser.MarkerParser().get_markers(args.extended_macros)

    # create the script context
    context = opscontext.OpsContext(
                                    args.logfile,
                                    checker,
                                    logger,
                                    args.default_engine,
                                    markers)

    # init and run the state machine
    a = statemachine.StateMachine(statemachine.InitState(context))
    if a.run_all(context.extract_operations()) is False:
        sys.exit(1)


if __name__ == "__main__":
    main()
