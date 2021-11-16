//
// Copyright 2019-2020, Intel Corporation
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

// This file contains common functionality shared between our PMDK-unittests-* pipelines

PMDK_0_DIR = 'pmdk_0'
PMDK_1_DIR = 'pmdk_1'
PMDK_0_PATH = "${WORKSPACE}/${PMDK_0_DIR}"
PMDK_1_PATH = "${WORKSPACE}/${PMDK_1_DIR}"
TEST_OUTPUT_FILE_0 = 'test_0.log'
TEST_OUTPUT_FILE_1 = 'test_1.log'
TEST_OUTPUT_PATH_0 = "${WORKSPACE}/output/${TEST_OUTPUT_FILE_0}"
TEST_OUTPUT_PATH_1 = "${WORKSPACE}/output/${TEST_OUTPUT_FILE_1}"

// define functions common for PMDK-unittests-* pipelines:

/**
 * Function that removes unwanted tests from PMDK repository.
 * @param blacklist List of tests or folders to remove separated by space.
 * @param pmdk_path List of paths to PMDK repositories.
 */
def remove_unwanted_tests(blacklist, pmdk_path) {
	libs.utils.echo_header("Workaround - remove blacklisted tests")
	libs.os.run_script("echo \"Removing: ${blacklist}\"")
	pmdk_path.each{
		def blacklisted_tests = blacklist.split(' ')
		def tests_path = it
		blacklisted_tests.each {
			def full_test_path = "${tests_path}/src/test/${it}"
			libs.os.rm(full_test_path)
		}
	}
}

def unload_unnecessary_modules() {
	libs.utils.echo_header("Unload unnecessary modules")
	libs.os.run_script("""
		sudo rmmod i40iw || true
	""")
}

def build_pmdk(pmdk_0_path, pmdk_1_path = null) {
	libs.utils.echo_header("make pmdk")
	if (pmdk_1_path != null) {
		libs.linux.run_bash_script_with_common_import("run_parallel 'build_pmdk --pmdk-path=${pmdk_0_path}' 'build_pmdk --pmdk-path=${pmdk_1_path}'")
	} else {
		libs.linux.run_bash_script_with_common_import("build_pmdk --pmdk-path=${pmdk_0_path}")
	}
}

/**
 * Function that executes python unittsest.
 * @param test_type Valgrind test type: none, drd, memcheck, helgrind, pmemcheck
 * @param pmdk_path List of paths to PMDK repositories.
 */
def run_py_tests(test_type, pmdk_path) {
	libs.utils.echo_header("run unittests_py-$test_type")

	pmdk_path.each {
		if (test_type == 'None') {
			libs.os.run_script("""
				sed -i 's/^    '"'"'force_enable'"'"': .*\$/    '"'"'force_enable'"'"': None,/' ${it}/src/test/testconfig.py
			""")
		} else {
			libs.os.run_script("""
				sed -i 's/^    '"'"'force_enable'"'"': .*\$/    '"'"'force_enable'"'"': '"'"'${test_type}'"'"',/' ${it}/src/test/testconfig.py
			""")
		}
	}
	if (pmdk_path.size() > 0){
		cmd_a0 = libs.linux.cmd_redir_out("cd ${pmdk_path.get(0)} && make test -j\$(nproc)", TEST_OUTPUT_PATH_0)
		cmd_a1 = libs.linux.cmd_redir_out("cd ${pmdk_path.get(1)} && make test -j\$(nproc)", TEST_OUTPUT_PATH_1)
		cmd_b0 = libs.linux.cmd_redir_out("cd ${pmdk_path.get(0)} && make pycheck", TEST_OUTPUT_PATH_0)
		cmd_b1 = libs.linux.cmd_redir_out("cd ${pmdk_path.get(1)} && make pycheck", TEST_OUTPUT_PATH_1)
		libs.linux.run_bash_script_with_common_import("""
			run_parallel "$cmd_a0" "$cmd_a1"
			run_parallel "$cmd_b0" "$cmd_b1"
		""")
	} else {
		libs.os.run_script("""
			cd ${pmdk_path.get(0)} && make test -j\$(nproc)
		""", TEST_OUTPUT_PATH_0)
	}
}

/**
 * Run PaJaC with proper parameters.
 */
def run_pajac(input_filepath, output_filepath) {
	dir(SCRIPTS_DIR) {
		libs.os.run_script("pajac/converter.py ${input_filepath} ${output_filepath}")
	}
}

/**
 * Run zip logs with proper parameters.
 * @param pmdk_dirs List of paths to PMDK repositories.
 * @param zip_dir Directory to save zip file with logs
 */
def run_zip_logs(pmdk_dirs, zip_dir) {
	def call_options = "--zip-dir ${zip_dir}"
	pmdk_dirs.each {
		call_options += " --pmdk-dir ${it}"
	}
	dir(SCRIPTS_DIR) {
		libs.os.run_script("testTools/zipUnittestsLogs.py ${call_options}")
	}
}

def post_always() {
	def results = []

	try {
		libs.utils.write_os_and_branch(params.LABEL, params.BRANCH)

		def all_test_logs_to_parse = []
		def all_pmdk_paths = []
		if (TEST_OUTPUT_PATH_0){
			echo "found ${TEST_OUTPUT_PATH_0}"
			all_test_logs_to_parse.add(TEST_OUTPUT_PATH_0)
			all_pmdk_paths.add(PMDK_0_PATH)
		}
		if (TEST_OUTPUT_PATH_1){
			echo "found ${TEST_OUTPUT_PATH_1}"
			all_test_logs_to_parse.add(TEST_OUTPUT_PATH_1)
			all_pmdk_paths.add(PMDK_1_PATH)
		}

		if (all_test_logs_to_parse.any()) {
			// prepare to parsing - combine all test logs into one
			def all_logs_content_combined = ""
			all_test_logs_to_parse.each { all_logs_content_combined += readFile(it) }
			def combined_logs_filename = "combined_logs.log"
			dir("${WORKSPACE}/${libs.utils.OUTPUT_DIR_NAME}") {
				writeFile(file: combined_logs_filename, text: all_logs_content_combined)
			}

			// run converter on combined log file
			def junit_xml_path = "${libs.utils.RESULTS_DIR_NAME}/junit.xml"
			this.run_pajac("${libs.utils.OUTPUT_DIR}/${combined_logs_filename}", "${WORKSPACE}/${junit_xml_path}")
			results.add(junit(junit_xml_path))

			// zip log files
			this.run_zip_logs(all_pmdk_paths, "${WORKSPACE}/${libs.utils.RESULTS_DIR_NAME}")

			// upload logs to artifactory
			// Temporary commented out until better connection setup to artifactory
			//libs.utils.send_files_to_artifactory("${WORKSPACE}/${libs.utils.RESULTS_DIR_NAME}/detailed_logs_*.zip")
		}
	}
	finally {
		libs.utils.archive_results_and_output()
		libs.os.unset_jenkins_warning_on_dut()
		if (params.SEND_RESULTS) {
			libs.utils.send_test_summary_via_mail(results)
			libs.utils.send_test_summary_to_webpage(results)
		}
	}
}


// below is necessary in order to closure work properly after loading the file in the pipeline:
return this