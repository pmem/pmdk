# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#
import logging
from os import path
import shlex
import subprocess as sp
import tempfile


DEFAULT_START_COMMANDS = ["set width 0\n",
                          "set height 0\n",
                          "set verbose off\n",
                          "set confirm off\n"]


class GdbProcess:
    """Class for invoking gdb from python program

       It writes and executes commands from a temporary gdb command file.

       Attributes:
           _process (CompletedProcess): finished process returned from run().

    """
    def __init__(self, path_to_test_dir, path_to_exec, args_to_exec="",
                 gdb_options="", timeout=15):
        """
        Args:
            path_to_test_dir (string): path to directory containing test,
                which invoked this script. Used for saving log file.
            path_to_exec (string): path to executable for gdb subprocess.
            args_to_exec (string): arguments to executable, divided by spaces.
                Defaults to empty string.
            gdb_options (string): options for gdb, divided by spaces.
                Defaults to empty string.
            timeout (integer): timeout in seconds. Defaults to 15.
            commands (list): list of commands to gdb command file.
                Always contains default commands at its beginning.

        """
        self._process = None
        self.path_to_exec = path_to_exec
        self.args_to_exec = args_to_exec
        self.gdb_options = gdb_options
        self.timeout = timeout
        self.commands = DEFAULT_START_COMMANDS[:]

        logging.basicConfig(filename=path.join(path_to_test_dir,
                            "python_gdb.log"),
                            filemode="w", level=logging.INFO)

    def execute(self):
        self.file_gdb = tempfile.NamedTemporaryFile(mode="r+")
        self.file_gdb.writelines("%s" % command for command in self.commands)
        self.file_gdb.seek(0)
        run_list = self._prepare_args()

        try:
            self._process = sp.run(run_list,
                                   stdin=sp.PIPE,
                                   stdout=sp.PIPE,
                                   stderr=sp.PIPE,
                                   timeout=self.timeout)
        except sp.TimeoutExpired:
            raise

        logging.info(self._process.stdout)
        logging.error(self._process.stderr)
        self.file_gdb.close()

    def command(self, command):
        if not command.endswith("\n"):
            command = ''.join([command, "\n"])
        self.commands.append(command)

    def _prepare_args(self):
        args_str = ''.join(["gdb ", self.gdb_options,
                            " --command=", self.file_gdb.name,
                            " --args ", self.path_to_exec,
                            " ", self.args_to_exec])
        args = shlex.split(args_str)
        return args
