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

// This file contains common functions which are OS-agnostic

import java.util.regex.*
import jenkins.model.Jenkins

// declarations of common paths and filenames:
OUTPUT_DIR_NAME = 'output'
RESULTS_DIR_NAME = 'results'

OUTPUT_DIR = "${WORKSPACE}/${OUTPUT_DIR_NAME}"
RESULTS_DIR = "${WORKSPACE}/${RESULTS_DIR_NAME}"
SCRIPTS_DIR = "${WORKSPACE}/pmdk_files/scripts"

LOG_FILENAME = 'console.log'
LOG_FILE = "${OUTPUT_DIR}/${LOG_FILENAME}"

SYSTEM_INFO_FILENAME = 'system_info.txt'
SYSTEM_INFO_FILEPATH = "${RESULTS_DIR}/${SYSTEM_INFO_FILENAME}"

REVISION_VERSION_FILENAME = "sha.txt"

COMMON_FILE = "${SCRIPTS_DIR}/common.sh"

// list of known git revisions, should contains list of dicts with .url and .sha keys
known_git_revisions = []

/**
 * Enumeration class for distinction between test types.
 */
enum TestType {
	/**
	 * Value describing unittests set from PMDK repository.
	 */
	UNITTESTS('testconfig.sh', 'u'),

	/**
	 * Value describing tests for building, installing and using binaries from PKG (PMDK repository).
	 */
	PKG('testconfig.sh', 'r'),

	/**
	 * Value describing unittests set in python from PMDK repository.
	 */
	UNITTESTS_PY('testconfig.py', 'p'),

	/**
	 * Value describing test set from PMDK-test repository.
	 */
	PMDK_TESTS('config.xml', 't'),

	/**
	 * Value describing test set from PMDK-convert repository.
	 */
	PMDK_CONVERT('convertConfig.txt', 'c'),

	/**
	 * Value describing test set for FIO test.
	 */
	PMDK_FIO('', 'f')

	/**
	 * Constructor for assigning proper values to enumerators' properties.
	 * @param path_to_config Path to the config generated by createNamescpacesConfig.sh script.
	 * @param script_parameter Parameter which is passed to the createNamespacesConfig.sh script.
	 */
	TestType(String path_to_config, String script_parameter) {
		this.config_path = path_to_config
		this.script_parameter = script_parameter
	}

	/**
	 * Getter for property describing path to the config generated by createNamescpacesConfig.sh script.
	 */
	def get_config_path() {
		return this.config_path
	}

	/**
	 * Getter for property describing parameter which is passed to the createNamespacesConfig.sh script.
	 */
	def get_script_parameter() {
		return this.script_parameter
	}

	/**
	 * Property holding path to the config generated by createNamescpacesConfig.sh script.
	 */
	private final String config_path

	/**
	 * Property holding parameter which is passed to the createNamespacesConfig.sh script.
	 */
	private final String script_parameter
}

// "export" this enum type;
// below is necessary in order to closure work properly with enum after loading the file in the pipeline:
this.TestType = TestType


def get_current_job_build_url() {
	return "${env.JENKINS_URL}view/all/job/${currentBuild.projectName}/${currentBuild.id}/"
}

/**
 * Function which runs CMake.
 * @param path A path pointing to directory with CMakeLists.txt file.
 * @param parameters Additional parameters for CMake configuration.
 * @param additional_env_vars If some additional exports are needed before call CMake, put there here.
 */
def run_cmake(path, parameters = "", additional_env_vars = "") {
	if (additional_env_vars != "") {
		additional_env_vars = "export ${additional_env_vars} &&"
	}
	libs.os.run_script("${additional_env_vars} cmake ${path} ${parameters}")
}

/**
 * Function which runs make on as many threads as available cores.
 * @param parameters Additional parameters for CMake configuration.
 * @param additional_env_vars If some additional exports are needed before call make, put there here.
 */
def run_make(parameters = "", additional_env_vars = "") {
	if (additional_env_vars != "") {
		additional_env_vars = "export ${additional_env_vars} &&"
	}
	libs.os.run_script("${additional_env_vars} make -j\$(nproc) ${parameters}")
}

/**
 * Add repository revision to known revisions. This is used for example to send e-mail with test result.
 * If requested repository (matching URL and SHA) is already known, it is not added second time.
 * However - if, for example, provided repo was added previously with different SHA, it will be added as separate entry.
 * @param repo_url URL of repository.
 * @param repo_sha SHA of repository revision.
 */
def remember_git_revision(repo_url, repo_sha) {
	def current_repo_revision = [:]
	current_repo_revision.url = repo_url
	current_repo_revision.sha = repo_sha
	if(!this.known_git_revisions.contains(current_repo_revision)) {
		this.known_git_revisions.add(current_repo_revision)
	}
}

/**
 * Prints given text as a visible header in Jenkins log and console log.
 * @param text String to print in header.
 */
def echo_header(text) {
	def header = "*********************************** ${text} ***********************************"
	// echo to Jenkins log:
	echo header

	// echo to console output:
	libs.os.run_script("echo \"${header}\"")
}

/**
 * Download repository with git.
 * @param repo_url URL to requested repository.
 * @param branch Specified branch to checkout.
 * @param target_directory Directory to which will the repository be clonned. If not specified, repository will be cloned to current working directory.
 * @param credentials_id Jenkins' credential ID which will be used for clonning the repo. If null, no credentials will be be specified. Default null.
 * @param keep_sha If false, it disables storing the cloned repo SHA in the list.
 */
def clone_repository(repo_url, branch, target_directory = '', credentials_id = null, keep_sha = true) {
	def specified_target_dir = (target_directory == '') ? false : true

	// If target dir is not specified, then we will create temporary dir and then copy contents to current dir.
	// The reason is: checkout will remove all files from current dir if its not empty.
	if(!specified_target_dir) {
		target_directory = "repository_target_temp_${this.generate_random_string(8)}"
	}

	def user_remote_configs = [ url: repo_url ]
	if (credentials_id != null) {
		user_remote_configs.credentialsId = credentials_id
	}

	def revision = checkout([$class: 'GitSCM',
	                         branches: [[name: "${branch}"]],
	                         doGenerateSubmoduleConfigurations: false,
	                         extensions: [[$class: 'RelativeTargetDirectory',
	                                       relativeTargetDir: target_directory]],
	                         submoduleCfg: [],
	                         userRemoteConfigs: [ user_remote_configs ]])

	this.echo_header("Git log")
	dir(target_directory) {
		libs.os.run_script("git log --oneline -n 5")
		if (keep_sha) {
			this.remember_git_revision(repo_url, revision.GIT_COMMIT)
		}
	}
	dir(OUTPUT_DIR_NAME) {
		writeFile file: REVISION_VERSION_FILENAME, text: this.get_revision_version()
	}

	if(!specified_target_dir) {
		// if target dir should be './', move here all contents from target dir and remove empty dir
		libs.os.move_dir_content(target_directory, './')
		libs.os.run_script("rmdir ${target_directory}")
	}
}

/**
 * Download repository with git using clone_repository function but with later merging with target_branch.
 * @param repo_url URL to requested repository.
 * @param ghprbPullId Specified pull request to checkout.
 * @param target_dir Directory to which will the repository be clonned.
 * @param target_branch Specified branch to merge with checkout
 */
def clone_repository_with_pr(repo_url, ghprbPullId, target_dir, target_branch, keep_sha = true) {
	this.clone_repository(repo_url, '', target_dir, credentials_id = null, false)
	dir(target_dir) {
		libs.os.run_script("""
			git fetch --quiet origin pull/${ghprbPullId}/head:${ghprbPullId}branch
			git checkout --quiet ${ghprbPullId}branch
		""")
		if(keep_sha == true) {
			def sha = libs.os.run_script('git rev-parse HEAD').output
			this.remember_git_revision("${repo_url}/${ghprbPullId}branch", sha)
		}
		libs.os.run_script("""
			git diff --check origin/${target_branch} ${ghprbPullId}branch 2> ${libs.os.null_device()}
			git merge origin/${target_branch} --no-commit --no-ff 2> ${libs.os.null_device()}
		""")
	}

	dir(this.OUTPUT_DIR_NAME) {
		writeFile file: this.REVISION_VERSION_FILENAME, text: this.get_revision_version()
	}
}

/**
 * Generate random alphanumeric string.
 * @param length Number of characters to generate.
 */
def generate_random_string(length) {
	String chars = (('a'..'z') + ('A'..'Z') + ('0'..'9')).join('')
	Random rnd = new Random()
	def random_string = ""
	for(i = 0; i < length; i++) {
		random_string = random_string + chars.charAt(rnd.nextInt(chars.length()))
	}
	return random_string
}

/**
 * Generate random integer number.
 * @param lower_bound The lowest possible value from range, inclusive.
 * @param upper_bound The highest possible value from range, inclusive.
 */
def generate_random_int(lower_bound, upper_bound) {
	return Math.abs(new Random().nextInt() % (upper_bound - lower_bound)) + lower_bound
}

/**
 * Delay execution of pipeline for given number of seconds.
 * @param seconds Amount of seconds to wait.
 */
def sleep_seconds(seconds) {
	sleep(time: seconds, unit: 'SECONDS')
}

/**
 * Clone the pmdk-tests repository to the 'pmdk-tests' directory.
 * @param branch The branch (or tag or revision) for checking out to.
 */
def clone_pmdk_test(branch = "master") {
	this.clone_repository('https://github.com/pmem/pmdk-tests.git', branch, 'pmdk-tests')
}

/**
 * Print OS and BRANCH params of current job to Jenkins. Call script which collects system info and save it to logs.
 */
def system_info() {
	this.echo_header("system info")

	dir(RESULTS_DIR_NAME) {
		writeFile file: SYSTEM_INFO_FILENAME, text: ''
	}

	echo "OS: ${params.LABEL}"
	echo "Branch: ${params.BRANCH}"

	libs.os.run_systeminfo_script()
}

/**
 * Archive artifacts from output directory.
 */
def archive_output() {
	archiveArtifacts artifacts: "${OUTPUT_DIR_NAME}/**"
}

/**
 * Archive artifacts from output and results directory.
 */
def archive_results_and_output() {
	this.archive_output()
	archiveArtifacts artifacts: "${RESULTS_DIR_NAME}/**"
}

/**
 * Write 'result' file (success, fail) and archive it.
 * @param result String representing result (could be e.g. 'success' or 'fail').
 */
def write_result_and_archive(result) {
	dir(RESULTS_DIR_NAME) {
		writeFile file: "${result}.txt", text: result
	}
	archiveArtifacts artifacts: "${RESULTS_DIR_NAME}/**"
}

/**
 * Upload files to artifactory.
 * @param files_pattern Path pattern for files to upload.
 */
def send_files_to_artifactory(files_pattern) {
	def server = Artifactory.server 'artif-pmdk'
	def uploadSpec = """{
		"files": [ {
			"pattern": "${files_pattern}",
			"target": "persistentmemorydevkit-igk-local/",
			"flat": "false"
		} ]
		}"""
	server.upload spec: uploadSpec
}

/**
 * Write file with name OS_BRANCH.txt to results dir.
 */
def write_os_and_branch(os, branch = null) {
	def branch_chunk = (branch == null) ? "" : "_${branch}"
	def filename = "${os}${branch_chunk}.txt"
	dir(RESULTS_DIR_NAME) {
		writeFile file: filename, text: ''
	}
}

/**
 * Get week number of date when current build has started.
 * Assume that the start of the week in on Monday.
 * @return Integer value of ww.
 */
def get_work_week_of_build_start() {
    return this.get_work_week_from_timestamp(currentBuild.startTimeInMillis)
}

/**
 * Get work week number of the given timestamp.
 * Assume that the start of the week in on Monday.
 * @param timestamp_in_ms Unix timestamp (but in milliseconds).
 * @return Integer value of ww.
 */
def get_work_week_from_timestamp(timestamp_in_ms) {
    // Groovy sandbox does not allow to change properties of locale, default start of the week is on Sunday,
    // so we need to make a trick: shift the starting date by one day ahead - this will give us in the result proper week
    // number with starting on Monday.

    // Groovy sandbox does not allow creation of Date objects with constructor different than Unix milliseconds epoch,
    // so we cannot count in days but only in milliseconds
    // (24 hours / 1 day) * (60 minutes / 1 hour) * (60 seconds / 1 minute) * (1000 milliseconds / 1 second):
    def one_day_in_ms = 24 * 60 * 60 * 1000

    // get build start date object:
    def start_date = new Date(timestamp_in_ms + one_day_in_ms)

    // get week number and cast it to integer:
    return Integer.valueOf(start_date.format("w"))
}

/**
 * Return git revision string of all cloned repositories.
 * @return All clonned repos revisions in the form of one string.
 */
def get_revision_version() {
	def revision_string = ""
	this.known_git_revisions.each {
		revision_string += it.url + ":\n" + it.sha + "\n"
	}
	return revision_string
}

class TestsResult {
	Integer all = 0
	Integer passed = 0
	Integer failed = 0
	Integer skipped = 0
	private def build_info
	private def env

	TestsResult(results = [], def build_info, def env) {
		results.each {
			if (it != null) {
				this.all += it.getTotalCount()
				this.passed += it.getPassCount()
				this.failed += it.getFailCount()
				this.skipped += it.getSkipCount()
			}
		}
		this.build_info = build_info
		this.env = env
	}

	def get_html_summary() {
		$/${this.get_html_beginning()}
                <table id="t1">
                    <thead>
                        ${this.get_html_table_heading_row()}
                    </thead>
                    <tbody>
                        ${this.get_html_table_data_row()}
                    </tbody>
                </table>
            ${this.get_html_ending(this.env)}
        /$
	}

	static def get_html_summary_for_weekly(results_data, env) {
		def data_rows = ""
		results_data.each {
			def tests_results = new TestsResult([it.test_results], it.build_info, env)
			data_rows += tests_results.get_html_table_data_row()
		}
		def html_summary = $/${TestsResult.get_html_beginning()}
	                <table id="t1">
	                    <thead>
	                        ${TestsResult.get_html_table_heading_row()}
	                    </thead>
	                    <tbody>
	                        ${data_rows}
	                    </tbody>
	                </table>
	            ${TestsResult.get_html_ending(env)}
	        /$
		return html_summary
	}

	private def get_html_table_data_row() {
		def test_class = " class=\"${this.get_css_class_for_pipeline_results()}\""
		def failed_class = (this.failed > 0) ? " class=\"fails\"" : ""

		def urls =
				"<a href=\"${env.JENKINS_URL}blue/organizations/jenkins/${this.build_info.project_name}/detail/${this.build_info.project_name}/${this.build_info.id}/pipeline\">blue</a>, " + "<a href=\"${env.JENKINS_URL}blue/organizations/jenkins/${this.build_info.project_name}/detail/${this.build_info.project_name}/${this.build_info.id}/tests\">blue tests</a>, " + "<a href=\"${env.JENKINS_URL}job/${this.build_info.project_name}/${this.build_info.id}/testReport\">tests</a>, " + "<a href=\"${env.JENKINS_URL}job/${this.build_info.project_name}/${this.build_info.id}/\">build</a>, " + "<a href=\"${env.JENKINS_URL}job/${this.build_info.project_name}/${this.build_info.id}/consoleText\">console</a>, " + "<a href=\"${env.JENKINS_URL}job/${this.build_info.project_name}/${this.build_info.id}/artifact/results/system_info.txt\">system_info</a>"
		def work_week_of_build_start = this.get_work_week_from_timestamp(this.build_info.start_time_in_millis)
		def build_start = new Date(this.build_info.start_time_in_millis).format("dd.MM.yyyy HH:mm")
		return """
				<tr ${test_class}>
					<td>${this.build_info.project_name}</td>
					<td>${this.build_info.display_name}</td>
					<td>ww${work_week_of_build_start} ${build_start}</td>
					<td>${this.build_info.duration_string}</td>
					<td>${this.build_info.result}</td>
					<td>${urls}</td>
					<td>${this.all}</td>
					<td>${this.passed}</td>
					<td>${this.skipped}</td>
					<td ${failed_class}>${this.failed}</td>
					<td>${this.build_info.git_revisions_string}</td>
				</tr>
		"""
	}

	private def get_css_class_for_pipeline_results() {
		if(this.build_info.result == "SUCCESS") {
			return "success"
		}
		else if(this.build_info.result == "FAILURE") {
			return "failure"
		}
		else if(this.build_info.result == "UNSTABLE") {
			return "unstable"
		}
		else {
			return "unknown"
		}
	}


	private static def get_html_table_heading_row() {
		return """
			<tr>
				<th>Pipeline</th>
				<th>Build</th>
				<th>Starting time</th>
				<th>Duration</th>
				<th>Pipeline status</th>
				<th>URLs</th>
				<th>All tests</th>
				<th>Passed</th>
				<th>Skipped</th>
				<th>Failed</th>
				<th>Git commit</th>
			</tr>
		"""
	}

	private static def get_css_style() {
		$/
            <style>
                #t1 {
                  border: 3px solid #000000;
                  width: 100%;
                  text-align: center;
                  border-collapse: collapse;
                }
                #t1 th {
                  border: 1px solid #000000;
                  font-weight:bold;
                  background: #2c3f66;
                  color: white;
                  border-bottom: 3px solid #000000;
                  padding: 5px 4px;
                }
                #t1 td {
                  border: 1px solid #000000;
                  padding: 5px 4px;
                  border-bottom: 1px solid #000000;
                }
                #t1 .success {
                  background: #baffc9;
                }
                #t1 .failure {
                  background: #FEBCC8;
                }
                #t1 .unstable {
                  background: #ffffba;
                }
				#t1 .unknown {
                  background: #ffb3ba;
                }
                #t1 .fails {
                  background: red;
                  color: white;
                  font-weight:bold;
                }            
            </style>
        /$
	}

	private static def get_html_beginning() {
		$/<!DOCTYPE html>
        <html>
            <head>
                ${get_css_style()}
            </head>
            <body>
        /$
	}

	private static def get_html_ending(env) {
		$/
                <p>
                    ---<br />
                    generated automatically by <a href="${env.JENKINS_URL}">Jenkins</a>
                </p>
            </body>
        </html>
        /$
	}
}

// Required to use top-level function inside class method
TestsResult.metaClass.get_work_week_from_timestamp = { x -> get_work_week_from_timestamp(x) }

/**
 * Class holding basic data of Jenkins build.
 */
class BuildInfo implements Serializable {
	def project_name
	def id
	def display_name
	def duration_string
	def result
	def start_time_in_millis
	def git_revisions_string
	private def context

	/**
	 * Create new BuildInfo via asking Jenkins about specific build.
	 * @param name Name of the pipeline.
	 * @param id Number of the job.
	 * @param context Just pass here libs.utils, Jenkins is cripple. Need context of caller in order to call later copyArtifacts.
	 * @return new instance of BuildInfo. Probably you should afterwards call `fetch_sha_artifact` to get sha, as we can't
	 * call this function nor in the constructor nor in the static method, thanks to Jenkons serialisation restrictions.
	 */
	static def from_jenkins(name, id, context) {
		return new BuildInfo(name, id, context)
	}

	/**
	 * Create new BuildInfo from the current build.
	 * @param context Just pass here libs.utils, Jenkins is cripple. Need context of caller in order to get `currentBuild`.
	 * @return new instance of BuildInfo. Calling `fetch_sha_artifact` is not needed as current build should have
	 * properly filled `utils.known_git_revisions`.
	 */
	static def from_current(context) {
		return new BuildInfo(context.currentBuild.projectName,
		                     context.currentBuild.id,
		                     context.currentBuild.displayName,
		                     context.currentBuild.durationString,
		                     "${context.currentBuild.currentResult}",
		                     context.currentBuild.startTimeInMillis,
		                     context.get_revision_version())
	}

	/**
	 * Ask for sha.txt artifact and loads its content.
	 * It probably WON'T work for 'currentBuild' - it's designed to be used on finished jobs.
	 */
	def fetch_sha_artifact() {
		def temp_dir_for_artifacts = "temp_dir_for_artifacts_${this.project_name}_${this.id}"
		def sha_string = ""
		def copy_artifacts_params = [projectName: "${this.project_name}",
		                             selector: context.specific("${this.id}"),
		                             flatten: true,
		                             target: temp_dir_for_artifacts,
		                             filter: "output/sha.txt"]
		try {
			this.context.copyArtifacts(copy_artifacts_params)
			sha_string = this.context.readFile("${temp_dir_for_artifacts}/sha.txt")
		} catch (ex) { // silent exceptions: not existing sha.txt will simply result in empty sha string
		}
		this.git_revisions_string = sha_string
	}

	private BuildInfo(project_name,
	                  id,
	                  display_name,
	                  duration_string,
	                  result,
	                  start_time_in_millis,
	                  git_revisions_string) {

		this.project_name = "${project_name}"
		this.id = "${id}"
		this.display_name = "${display_name}"
		this.duration_string = "${duration_string}".replaceAll("and counting", "")
		this.result = "${result}"
		this.start_time_in_millis = "${start_time_in_millis}" as Long
		this.git_revisions_string = "${git_revisions_string}"
	}

	private BuildInfo(name, id, context) {
		def build_data = Jenkins.instance.getItemByFullName("${Hudson.instance.getJob(name).fullName}").getBuildByNumber(id)
		this.context = context
		this.project_name = name
		this.id = id
		this.display_name = "${build_data.displayName}"
		this.duration_string = "${build_data.durationString}"
		this.result = "${build_data.result}"
		this.start_time_in_millis = "${build_data.startTimeInMillis}" as Long
		this.git_revisions_string = "" // sha gathering has to be done separately via fetch_sha_artifact (thanks, Jenkins!)
	}
}

this.BuildInfo = BuildInfo

/**
 * Send summary with test results via email.
 * @param results List of objects returned by `junit` call.
 * @param build_info BuildInfo instance. If null, current build is considered as target.
 * @param recipients Addressee of the mail.
 */
def send_test_summary_via_mail(results = [], build_info = null, recipients = params.EMAIL_RECIPIENTS) {
	if (build_info == null) {
		build_info = this.BuildInfo.from_current(this)
	}
	def message_title = "[Jenkins/PMDK] Report ${build_info.project_name} ${build_info.display_name}, ww ${this.get_work_week_of_build_start()}"
	def message_body = new TestsResult(results, build_info, env).get_html_summary()

	mail(to: recipients,
	     subject: message_title,
	     body: message_body,
	     mimeType: "text/html")
}

/**
 * Send weekly summary with test results via email.
 * @param bundled List of dicts with bundled data: .build_info and .test_results
 * @param recipients Addressee of the mail.
 */
def send_weekly_summary_via_mail(bundled = [], recipients = params.EMAIL_RECIPIENTS) {
	def message_title = "[Jenkins/PMDK] Report weekly"
	def message_body = TestsResult.get_html_summary_for_weekly(bundled, env)
	mail(to: recipients,
	     subject: message_title,
	     body: message_body,
	     mimeType: "text/html")
}

/**
 * Send summary with test results to webpage: pmdk-val.pact.intel.com
 * @param content Contains test results (html)
 */

def send_test_summary_to_webpage(results = []) {
	println "Temporary disabled"
	// def k8s_user = 'tiller'
	// def k8s_host = '10.237.156.16'
	// def html_file_name = 'row.html'
	// def credentials_id = 'k8s-tiller'
	// def tests_results = new TestsResult(results, currentBuild, env)
	// def rowBody = tests_results.get_html_table_data_row()

	// writeFile file: html_file_name, text: rowBody
	// withCredentials([sshUserPrivateKey(credentialsId: credentials_id, keyFileVariable: 'keyfile')]) {
	// 	def sendFileCmd = "scp -o 'StrictHostKeyChecking no' -i ${keyfile} ${html_file_name} ${k8s_user}@${k8s_host}:/home/tiller/charts/pmdk-val/static"
	// 	run_bash_script(sendFileCmd)
	// 	run_bash_script_with_common_import("update_website_content_via_ssh ${keyfile} ${k8s_user} ${k8s_host}")
	// }
}

/**
 * Function for applying regular expression to a text.
 * @param text Text to apply regex to - input for a regex engine.
 * @param pattern Regex pattern to be searched by a regex engine.
 * @param groups List of expected group names to be extracted.
 * @param pattern_flags Flags for regex engine. Default: MULTILINE | COMMENTS
 * @return list of dictionaries with all matched groups
 * Example:
 * def input_string = "changed: 10.91.28.117\nok: 0.91.28.119"
 * def pattern = $/ ^(?<status>\w+):\s(?<ip>[\w.]+)$ /$
 * def groups = ["status", "ip"]
 * def result = apply_regex(input_string, pattern, groups)
 * // result will contain list of matches: [[status:changed, ip:10.91.28.117], [status:ok, ip:0.91.28.119]]
 */
@NonCPS
def apply_regex(text, pattern, groups, pattern_flags = Pattern.COMMENTS | Pattern.MULTILINE) {
	Matcher regex_matcher = Pattern.compile(pattern, pattern_flags).matcher(text);
	def found_matches = []
	while (regex_matcher.find()) {
		def current_match = [:]
		for (current_group in groups) {
			current_match[current_group] = regex_matcher.group(current_group)
		}
		found_matches.add(current_match)
	}
	return found_matches
}

/**
 * Preparing workspace to work: clearing workspace, creating output dir and console.log.
 */
def prepare_workspace() {
	echo "Clean workspace"
	echo "Clearing ${WORKSPACE} directory"
	deleteDir()

	echo "Creating ${OUTPUT_DIR} directory with ${LOG_FILENAME} file"
	dir(OUTPUT_DIR_NAME) {
		writeFile file: LOG_FILENAME, text: ''
	}
}

/**
 * Preparing workspace to work and cloning pmdk_files repo to pmdk_files directory.
 */
def prepare_workspace_with_pmdk_files() {
	this.prepare_workspace()
	this.clone_repository('https://gitlab.devtools.intel.com/pmdk/pmdk_files.git',
	                      pmdk_files_branch,
	                      'pmdk_files',
	                      'gitlab-user',
	                      false)
}

return this