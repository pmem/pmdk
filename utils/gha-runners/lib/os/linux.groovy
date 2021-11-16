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

// This file contains Linux exclusive API

/**
 * Enumeration for linux distributions.
 */
enum DistroName {

	UNKNOWN(''),
	DEBIAN('Debian GNU/Linux'),
	CENTOS('CentOS Linux'),
	OPENSUSE('openSUSE Leap'),
	FEDORA('Fedora'),
	UBUNTU('Ubuntu'),
	RHEL('Red Hat Enterprise Linux'),
	RHELS('Red Hat Enterprise Linux Server')

	/**
	 * Constructor for assigning proper values to enumerators' properties.
	 * @param name Name of the linux distribution.
	 */
	DistroName(String distro_name) {
		this.distro_name = distro_name
	}

	def to_string() {
		return this.distro_name
	}

	static def from_string(string_distro_name) {
		// lookup in all enum labels and check if given string matches:
		for (DistroName distro_name : DistroName.values()) {
			if (distro_name.to_string() == string_distro_name) {
				return distro_name
			}
		}
		return DistroName.UNKNOWN
	}

	private final String distro_name
}

// "export" this enum type;
// below is necessary in order to closure work properly with enum after loading the file in the pipeline:
this.DistroName = DistroName

/**
 * Function returning DistroName object of the current node.
 * @return DistroName enum instance
 */
def get_distro() {
	distro = sh(script:
	"""#!/usr/bin/env bash
		source ${this.COMMON_FILE}
		check_distro
	""", returnStdout: true).trim()
	return this.DistroName.from_string(distro)
}

/**
 * Function returning Ip addres of chosen network card.
 * @param nic_name Name of the Network card model.
 * @return ip_addr Ip address of a chosen NIC
 */
def get_nic_ip_address(nic_name) {
	ip_addr = libs.os.run_script("""
		nic_id=\$(sudo lshw -C NETWORK -short | grep ${nic_name} | awk '{print \$2}' | head -n 1)
		ip -4 -j a | jq -r ".[] | select(.ifname==\\"\$nic_id\\").addr_info[].local"
	""").output
	return ip_addr
}

/**
 * Function which runs a script in bash and logs it's output.
 *
 * @param script_text Bash script's contents. No need for shebang, pass only what to do.
 * @param log Path to the file where script's output will be redirected. If empty string, then no output redirection will be used. Default set to LOG_FILE.
 * @param import_common_file Boolean flag, if set to true the 'common.sh' script will be included.
 * @param error_on_non_zero_rc Boolean flag, if set to true function will raise error when command returns non-zero return code.
 * @return map object containing '.output' with command's output and '.status' with returned command's status code
 */
def run_bash_script(script_text, log_file = libs.utils.LOG_FILE, error_on_non_zero_rc = true, import_common_file = false) {
	// first, prepare optional portions of the script:
	// redirect all script's output to the files:
	// 1. Separate log file for this current command exclusively, with random name.
	// 2. If requested - append to given log file.
	def current_script_output_file = "${WORKSPACE}/command_${libs.utils.generate_random_string(20)}.log"
	def redirect_script = """
		# ensure that this command output file is empty:
		echo "" > ${current_script_output_file}
		# ensure that log_file exists before using `tee -a`:
		touch ${log_file} ${current_script_output_file}
		# redirect stdout to the named pipe:
		exec > >(tee -a ${log_file} ${current_script_output_file})
		# merge stderr stream with stdout:
		exec 2>&1
	"""

	// import things defined in common.sh file if not set otherwise:
	def source_script = (import_common_file == false) ? "" : "source ${libs.utils.COMMON_FILE}"

	def full_script_body =  """#!/usr/bin/env bash
		set -o pipefail
		${redirect_script}
		${source_script}
		# now do whatever user wanted to:
		${script_text}
	"""
	// second, do the actual call:
	def returned_status = sh(script: full_script_body, returnStatus: true)

	// third, capture script output from file:
	def script_output = readFile current_script_output_file

	// delete the log file:
	sh "rm -f ${current_script_output_file}"

	// capturing status is also disabling default behavior of `sh` - error when exit status != 0
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
 *  Function which runs a script in bash and logs it's output. There will be always 'common.sh' script included.
 * @param script_text Bash script's contents. No need for shebang, pass only what to do.
 * @param log Path to the file where script's output will be redirected. If empty string, then no output redirection will be used. Default set to LOG_FILE.
 * @param error_on_non_zero_rc Boolean flag, if set to true function will raise error when command returns non-zero return code.
 * @return object containing '.output' with command's output and '.status' with returned command's status code
 */
def run_bash_script_with_common_import(script_text, log = libs.utils.LOG_FILE, error_on_non_zero_rc = true) {
	return this.run_bash_script(script_text, log, error_on_non_zero_rc, true)
}

/**
 * Return bash command with redirection stdout and stderr to file.
 * @param cmd Command to redirect the output from.
 * @param output_path File to redirect the output to.
 * @return command with redirection in the form of string.
 */
def cmd_redir_out(cmd, output_path) {
	return "${cmd} 2>&1 | tee -a ${output_path}"
}

return this