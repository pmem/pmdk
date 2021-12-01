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


class UniqueTestNameProvider:
    """ Class guarding that all the testcases will have unique names. """
    test_names = []

    @staticmethod
    def clear():
        UniqueTestNameProvider.test_names.clear()

    @staticmethod
    def provide(test_group_name: str, test_name: str, test_parameters: str = None):
        """ Static method for returning unique test name. It appends:
        "(run number #)" and parameters (if supplied) to the test name."""

        if test_parameters is not None:
            parameter_string = " with parameters: " + test_parameters
        else:
            parameter_string = ""

        suffix_iterator = 0
        while True:
            new_test_name = test_name + " (run number " + str(suffix_iterator) + ")" + parameter_string
            new_full_test_name = test_group_name + new_test_name
            suffix_iterator += 1
            if new_full_test_name not in UniqueTestNameProvider.test_names:
                UniqueTestNameProvider.test_names.append(new_full_test_name)
                return new_test_name
