# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#
import logging
import os
import shlex
import subprocess as sp
import tempfile
from os import path
import futils

DEFAULT_START_COMMANDS = ['set disable-randomization off',
                          'set width 0',
                          'set height 0',
                          'set verbose off',
                          'set confirm off']


class GdbProcess:
    """Class for invoking gdb from python program

       It writes and executes commands from a temporary gdb command file.

       Attributes:
           _process (CompletedProcess): finished process returned from run().
           _env (dict): environment variables

    """
    def __init__(self, env, cwd, testnum, path_to_exec, args_to_exec='',
                 gdb_options='', timeout=15):
        """
        Args:
            cwd (string): path to the test directory.
            testnum (string): test number.
            path_to_exec (string): path to executable for gdb subprocess.
            args_to_exec (string): arguments to executable, divided by spaces.
                Defaults to empty string.
            gdb_options (string): options for gdb, divided by spaces.
                Defaults to empty string.
            timeout (integer): timeout in seconds. Defaults to 15.
            commands (list): list of commands to gdb command file.
                Always contains default commands at its beginning.
            log_file (str): path to the gdb output log file.

        """
        self._process = None
        self._env = env
        self.cwd = cwd
        self.path_to_exec = path_to_exec
        self.args_to_exec = args_to_exec
        self.gdb_options = gdb_options
        self.timeout = timeout
        self.commands = []

        for command in DEFAULT_START_COMMANDS:
            self.add_command(command)

        log_name = 'python_gdb{}.log'.format(testnum)
        self.log_file = path.join(cwd, log_name)

        logging.basicConfig(filename=self.log_file, level=logging.INFO,
                            format='%(levelname)s:%(message)s')

    def execute(self):
        tmp = self._env.copy()
        futils.add_env_common(tmp, os.environ.copy())

        self.file_gdb = tempfile.NamedTemporaryFile(mode='r+')
        self.file_gdb.writelines('%s' % command for command in self.commands)
        self.file_gdb.seek(0)
        run_list = self._prepare_args()

        try:
            self._process = sp.run(args=run_list,
                                   env=tmp,
                                   stdin=sp.PIPE,
                                   stdout=sp.PIPE,
                                   stderr=sp.PIPE,
                                   universal_newlines=True,
                                   timeout=self.timeout)
        except sp.TimeoutExpired:
            self._close()
            raise

        logging.info(self._process.stdout)
        logging.error(self._process.stderr)

        self.validate_gdb()
        self._close()

    def add_command(self, command):
        if not command.endswith('\n'):
            command = ''.join([command, '\n'])
        self.commands.append(command)

    def validate_gdb(self):
        """Check if there was an error in gdb process"""
        if path.isfile(self.log_file + '.match'):
            # if there is a gdb log match file, do nothing - log file
            # will be checked by 'match' tool
            return
        if self._process.stderr:
            print(self._process.stderr)
            raise futils.Fail('Gdb validation failed')

    def _prepare_args(self):
        gdb_command = ''.join(['--command=', self.file_gdb.name])
        args_str = ' '.join(['gdb', self.gdb_options,
                             gdb_command, self.file_gdb.name,
                             '--args', self.path_to_exec,
                             self.args_to_exec])
        args = shlex.split(args_str)
        return args

    def _close(self):
        for handler in logging.root.handlers[:]:
            handler.close
            logging.root.removeHandler(handler)
        self.file_gdb.close()
