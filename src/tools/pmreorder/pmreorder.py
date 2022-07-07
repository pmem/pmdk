# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2022, Intel Corporation

import argparse
import statemachine
import opscontext
import consistencycheckwrap
import loggingfacility
import markerparser
import sys
import reorderengines
import signal

# Prevent printing python stacktrace on SIGINT. To achive that throw
# "SystemExit exception" instead of "KeyboardInterrupt exception".
signal.signal(signal.SIGINT, lambda signum, frame: exit(signum))


def main():
    pmreorder_version = "unknown"

    """
    Argv[1] should be given in order to use -v or --version flag. It is passed
    from the installation script. We check whether argv[1] was given. If it's
    not any of regular parameters - we use it as a version of pmreorder and
    remove it from the arguments list.
    """
    if len(sys.argv) > 1 and sys.argv[1][0] != "-":
        pmreorder_version = sys.argv[1]
        del sys.argv[1]

    # TODO unicode support
    # TODO parameterize reorder engine type
    parser = argparse.ArgumentParser(description="Store reordering tool")
    parser.add_argument(
        "-l",
        "--logfile",
        required=True,
        help="the pmemcheck log file to process",
    )
    parser.add_argument(
        "-c",
        "--checker",
        choices=consistencycheckwrap.checkers,
        default=consistencycheckwrap.checkers[0],
        help="choose consistency checker type",
    )
    parser.add_argument(
        "-p",
        "--path",
        required=True,
        help="path to the consistency checker and arguments. "
        + "Note: If program is given, the program has to take "
        + "a file name as the last parameter.",
        nargs="+",
    )
    parser.add_argument(
        "-n",
        "--name",
        help="consistency check function for the 'lib' checker. Note: "
        + "The function has to take a file name as the only parameter.",
    )
    parser.add_argument("-o", "--output", help="set the logger output file")
    parser.add_argument(
        "-e",
        "--output-level",
        choices=loggingfacility.log_levels,
        help="set the output log level",
    )
    parser.add_argument(
        "-x",
        "--extended-macros",
        help="list of pairs MARKER=ENGINE or json config file",
    )
    parser.add_argument(
        "-v",
        "--version",
        help="print version of the pmreorder",
        action="version",
        version="%(prog)s " + pmreorder_version,
    )
    engines_keys = list(reorderengines.engines.keys())
    parser.add_argument(
        "-r",
        "--default-engine",
        help="set default reorder engine default=NoReorderNoChecker",
        choices=engines_keys,
        default=engines_keys[0],
    )
    args = parser.parse_args()
    logger = loggingfacility.get_logger(args.output, args.output_level)
    checker = consistencycheckwrap.get_checker(
        args.checker, " ".join(args.path), args.name, logger
    )

    markers = markerparser.MarkerParser().get_markers(args.extended_macros)

    # create the script context
    context = opscontext.OpsContext(
        args.logfile, checker, logger, args.default_engine, markers
    )
    logger.debug("Input parameters: {}".format(context.__dict__))

    # init and run the state machine
    a = statemachine.StateMachine(statemachine.InitState(context))
    if a.run_all(context.extract_operations()) is False:
        sys.exit(1)


if __name__ == "__main__":
    main()
