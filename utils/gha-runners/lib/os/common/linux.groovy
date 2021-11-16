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

// This file contains Linux implementation of the common Windows and Linux API
// Every function / class from this file should have its Windows equivalent in the common/windows.groovy file

/**
 * Return the path to a null device in Linux.
 */
def null_device() {
    return "/dev/null"
}

/**
 * Call createNamespaceConfig.sh and print created configuration files.
 * @param conf_path Path to directory where config files will be created.
 * @param nondebug_lib_path path to installed pmdk libraries. Required if the tests have to be run with precompiled binaries.
 */
def create_namespace_and_config(conf_path, test_type, second_conf_path = '', nondebug_lib_path = '') {
	libs.utils.echo_header("Setup & create config")

	def dual_config_string = (second_conf_path == '') ? '' : "--dual-ns --conf-path_1=${second_conf_path}"

	libs.linux.run_bash_script_with_common_import("""
		mkdir --parents ${conf_path}
		${SCRIPTS_DIR}/createNamespaceConfig.sh -${test_type.get_script_parameter()} --conf-pmdk-nondebug-lib-path=${
		nondebug_lib_path} --conf-path_0=${conf_path} ${dual_config_string}
	""")

	if (test_type.get_config_path() != '') {
		libs.utils.echo_header(test_type.get_config_path())
		this.run_script("cat ${conf_path}/${test_type.get_config_path()}")
	}
	if (second_conf_path != '') {
		this.run_script("cat ${second_conf_path}/${test_type.get_config_path()}")
	}
}

/**
 * Move all contents from directory, including hidden files.
 * @param source_dir Directory from where content will be moved out.
 * @param target_dir Directory where content will be put to.
 */
def move_dir_content(source_dir, target_dir) {
    this.run_script("""
        # move all contents including hidden files:
        mv -f ${source_dir}/{.[!.],}* ${target_dir}
    """)
}

/**
 * Remove file.
 * @param target Target file to remove. Could contains wildcard '*'.
 */
def rm(target) {
    this.run_script("rm --force --recursive ${target}")
}

/**
 * Function which runs a script in bash and logs it's output.
 *
 * @param script_text Bash script's contents. No need for shebang, pass only what to do.
 * @param log Path to the file where script's output will be redirected. If empty string, then no output redirection will be used. Default set to LOG_FILE.
 * @param error_on_non_zero_rc Boolean flag, if set to true function will raise error when command returns non-zero return code.
 * @return map object containing '.output' with command's output and '.status' with returned command's status code
 */
def run_script(script_text, log_file = libs.utils.LOG_FILE, error_on_non_zero_rc = true) {
    return libs.linux.run_bash_script(script_text, log_file, error_on_non_zero_rc)
}

/**
 * Run script which will write systeminfo to libs.utils.SYSTEM_INFO_FILEPATH file.*/
def run_systeminfo_script() {
    libs.linux.run_bash_script_with_common_import("""
        system_info 2>&1 | tee -a ${libs.utils.SYSTEM_INFO_FILEPATH}
    """)
}

/**
 * Set the Doge as a welcome message after logging to the DUT, which will warn about running execution.*/
def set_jenkins_warning_on_dut() {
    libs.linux.run_bash_script_with_common_import("set_warning_message")
}

/**
 * Restore default welcome message after logging to the DUT which means that Jenkins' execution is done.*/
def unset_jenkins_warning_on_dut() {
    libs.linux.run_bash_script_with_common_import("disable_warning_message")
}

return this