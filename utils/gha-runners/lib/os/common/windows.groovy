//
// Copyright 2020, Intel Corporation
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in
//       the documentation and/or other materials provided with the
//       distribution.
//
//     * Neither the name of the copyright holder nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

// This file contains Windows implementation of the common Windows and Linux API
// Every function / class from this file should have its Linux equivalent in the common/linux.groovy file

/**
 * Return the path to a null device in Windows.
 */
def null_device() {
    return "\$null"
}

/**
 * Call createNamespaceConfig.sh and print created configuration files.
 * @param conf_path Path to directory where config files will be created.
 * @param nondebug_lib_path path to installed pmdk libraries. Required if the tests have to be run with precompiled binaries.
 */
def create_namespace_and_config(conf_path, test_type, second_conf_path = '', nondebug_lib_path = '') {
    echo "create_namespace_and_config not implemented yet"
}

/**
 * Move all contents from directory, including hidden files.
 * @param source_dir Directory from where content will be moved out.
 * @param target_dir Directory where content will be put to.
 */
def move_dir_content(source_dir, target_dir) {
    this.run_script("""
        # move all contents including hidden files:
        Get-ChildItem -Path "${source_dir}" -Recurse -Force | Move-Item -Destination "${target_dir}"
    """)
}

/**
 * Remove file.
 * @param target Target file to remove. Could contains wildcard '*'.
 */
def rm(target) {
    powershell("rm -Force -Recurse ${target}")
}

/**
 * Function which runs a script in Powershell and logs it's output.
 *
 * @param script_text Powershell script's contents.
 * @param log Path to the file where script's output will be redirected. If empty string, then no output redirection will be used. Default set to LOG_FILE.
 * @param error_on_non_zero_rc Boolean flag, if set to true function will raise error when command returns non-zero return code.
 * @return map object containing '.output' with command's output and '.status' with returned command's status code
 */
def run_script(script_text, log_file = libs.utils.LOG_FILE, error_on_non_zero_rc = true) {
    // Execution Windows script is done via saving script's content to file and running that script with redirected all
    // output to log files: one with random name to capture command output from and one with name requested by user.
    def random_string = libs.utils.generate_random_string(20)
    def current_script_output_file = "${WORKSPACE}/command_output_${random_string}.log"
    def current_script_execution_file = "${WORKSPACE}/command_script_${random_string}.ps1"

    // ensure that this command output file is empty:
    writeFile file: current_script_output_file, text: ""

    // save script's content to file:
    writeFile file: current_script_execution_file, text: script_text

    // prepare redirection to current command's output file:
    def redirect_output = / *>&1 | Tee-Object -Append -FilePath \"${current_script_output_file}\" /
    // prepare redirection to user requested log file:
    if(log_file != "") {
        redirect_output += / | Tee-Object -Append -FilePath \"${log_file}\" /
    }

    // now do the actual call:
    def call_command = /pwsh -Command "powershell -File \"${current_script_execution_file}\" ${redirect_output}"/
    def returned_status = bat(script: call_command, returnStatus: true)

    // capture script's output from file:
    def script_output = readFile current_script_output_file

    // delete the log file:
    powershell("rm -Force ${current_script_output_file}, ${current_script_execution_file}")

    // capturing status is also disabling default behavior of `powershell` - error when exit status != 0
    // here restore that behavior if not requested otherwise:
    if (error_on_non_zero_rc && returned_status != 0) {
        error("script returned exit code ${returned_status}")
    }

    def retval = [:]
    retval.output = script_output.trim()
    retval.status = returned_status
    return retval
}

/**
 * Run script which will write systeminfo to libs.utils.SYSTEM_INFO_FILEPATH file.*/
def run_systeminfo_script() {
    echo "run_systeminfo_script not implemented yet"
}

/**
 * Set the Doge as a welcome message after logging to the DUT, which will warn about running execution.*/
def set_jenkins_warning_on_dut() {
    echo "set_jenkins_warning_on_dut not implemented yet"
}

/**
 * Restore default welcome message after logging to the DUT which means that Jenkins' execution is done.*/
def unset_jenkins_warning_on_dut() {
    echo "unset_jenkins_warning_on_dut not implemented yet"
}

return this