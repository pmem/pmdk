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

// This file contains common API for RAS US tests

// label of node with turned on Error Injecting in BIOS
RAS_US_NODE_LABEL = "RAS_US"

/**
 * Simple data-structure class holding IP and unique label of node from Jenkins.*/
class SimpleNode {
    String label
    String ip

    SimpleNode(String label, String ip) {
        this.label = new String(label)
        this.ip = new String(ip)
    }

    /**
     * Implement comparison operator.
     * Check whether this instance is equal to other instance by comparing values
     * of stored label and ip.
     * @param other Instance to compare.
     * @return true if ip and labes are equal, false otherwise
     */
    boolean equals(SimpleNode other) {
        return this.ip == other.ip && this.label == other.label
    }
}
this.SimpleNode = SimpleNode

/**
 * Function for selecting certain amount of nodes with requested label among all non-busy Jenkins nodes.
 * If not found requested nodes, throws an exception.
 * @param requested_labels List of labels of node to search for. Node should have all the labels.
 * @param requested_node_count How many nodes needs to be selected.
 * @return List of {@link SimpleNode SimpleNode} objects.
 */
def select_active_nodes_with_labels(requested_labels, requested_node_count) {
    echo "Looking for ${requested_node_count} node(s) with labels ${requested_labels}."
    def selected_nodes = []
    if (requested_node_count < 1) {
        return selected_nodes
    }

    def jenkins_instance = jenkins.model.Jenkins.instance
    def found_nodes_count = 0
    for (current_node in jenkins_instance.nodes) {
        // if for some reason cannot get computer object, skip that node:
        if (current_node.toComputer() == null) {
            continue
        }

        // check only nodes which are online and are not busy:
        if (!current_node.toComputer().isOffline() && current_node.toComputer().countBusy() == 0) {
            // construct list with all labels of current node:
            def current_node_all_labels = []
            current_node.assignedLabels.each { current_node_all_labels.add(new String(it.getName())) }

            // check if current node has all of the requested labels:
            def node_has_all_requested_labels = current_node_all_labels.containsAll(requested_labels)
            if (node_has_all_requested_labels) {
                selected_nodes.add(new SimpleNode(current_node.selfLabel.getName(), current_node.toComputer().launcher.host))
                ++found_nodes_count
                echo "${found_nodes_count}/${requested_node_count} nodes found: ${current_node.selfLabel.getName()}"
                if (found_nodes_count == requested_node_count) {
                    break
                }
            }
        }
    }
    if (found_nodes_count < requested_node_count) {
        throw new Exception("Required count of nodes with labels ${requested_labels} not found! Requested ${requested_node_count} node(s), found ${found_nodes_count}.")
    }
    selected_nodes.each { echo "Selected node: ${it.label} / ${it.ip}" }
    return selected_nodes
}

/**
 * Function for selecting certain amount of nodes with requested label among all non-busy Jenkins nodes.
 * If not found requested nodes, waits until requested nodes will be available.
 * @param requested_labels List of labels of node to search for. Node should have all the labels.
 * @param requested_node_count How many nodes needs to be selected.
 * @return List of {@link SimpleNode SimpleNode} objects.
 */
def wait_for_active_nodes_with_labels(requested_labels, requested_node_count) {
    def all_requested_nodes_selected = false
    def selected_nodes_first_try = []
    while (!all_requested_nodes_selected) {
        try {
            echo "Waiting for ${requested_node_count} node(s) with labels ${requested_labels} will be available"
            selected_nodes_first_try = select_active_nodes_with_labels(requested_labels, requested_node_count)

            // Generate short random interval after which we'll double check
            // whether selected nodes are really free and are not taken by
            // other concurrent RAS job. This will prevent most of the time hazards when
            // two or more RAS jobs will be started at the same time (e.g. in weekly)
            // and they will selecting nodes at the same moment.
            def double_check_interval = libs.utils.generate_random_int(1, 30)
            echo "Sleeping ${double_check_interval} seconds in order to ensure selected nodes are free"
            libs.utils.sleep_seconds(double_check_interval)
            def selected_nodes_second_try = select_active_nodes_with_labels(requested_labels, requested_node_count)

            // if nodes from the first and second try are the same, we treat them as free
            // real estate and if only requested node count is matching, we're good to go!
            def are_selected_nodes_the_same = selected_nodes_first_try == selected_nodes_second_try
            def are_selected_all_requested_nodes = selected_nodes_first_try.size() == requested_node_count
            all_requested_nodes_selected = are_selected_nodes_the_same && are_selected_all_requested_nodes
        } catch (Exception ex) {
            all_requested_nodes_selected = false
            echo "${ex.message}"
            def waiting_interval = 20
            libs.utils.sleep_seconds(waiting_interval)
        }
    }
    return selected_nodes_first_try
}

/**
 * Function for setting nodes offline on Jenkins.
 * @param nodes List of SimpleNode objects which should be set offline.
 * @param reason Reason of bringing DUT offline - this will appear on Jenkins as a description why the DUT is offline.
 */
def put_nodes_offline(nodes, reason) {
    echo "Bring nodes offline with reason: '${reason}'"
    def jenkins_instance = jenkins.model.Jenkins.instance
    for (jenkins_node in jenkins_instance.nodes) {
        if (nodes.any { it.label == jenkins_node.selfLabel.getName() }) {
            // if - for some reason - node was unconnected on Jenkins, connect it before set offline state:
            jenkins_node.toComputer().connect(true)
            jenkins_node.toComputer().setTemporarilyOffline(true, new hudson.slaves.OfflineCause.ByCLI(reason))
            echo "Bringing DUT '${jenkins_node.selfLabel.getName()}' offine"
        }
    }
}

/**
 * Function for setting nodes online on Jenkins.
 * @param nodes List of SimpleNode objects which should be set online.
 */
def put_nodes_online(nodes) {
    echo "Bring nodes online"
    def jenkins_instance = jenkins.model.Jenkins.instance
    for (jenkins_node in jenkins_instance.nodes) {
        if (nodes.any { it.label == jenkins_node.selfLabel.getName() }) {
            jenkins_node.toComputer().setTemporarilyOffline(false, null)
            // connect node after setting it online - this will launch Jenkins agent on DUT if was not launched:
            jenkins_node.toComputer().connect(true)
            echo "Bringing DUT '${jenkins_node.selfLabel.getName()}' online"
        }
    }
}

/**
 * Copy XML logs from execution to workspace, archive it and load with Jenkins.
 * @param log_files list of paths to XML files
 * @return list of test results loaded by Jenkins
 */
def archive_logs(log_files) {
    def test_results = []
    if (!log_files.isEmpty()) {
        log_files.each {
            def source_file = new File(it.replaceAll(/\\+/, '/'))   // replaceAll is used as a workaround for Windows path convention.
            def content = ""
            dir(source_file.parentFile.absolutePath) {
                content = readFile(new File(source_file.name).toPath().toString())
            }
            dir(libs.utils.RESULTS_DIR_NAME) {
                writeFile file: source_file.name, text: content
            }
        }
        archiveArtifacts artifacts: "${libs.utils.RESULTS_DIR_NAME}/*.*"
        test_results.add(junit("**/${libs.utils.RESULTS_DIR_NAME}/*.xml"))
    }
    return test_results
}

/**
 * Archive sha.txt file with git revisions.
 */
def archive_sha() {
    dir(libs.utils.OUTPUT_DIR_NAME) {
        writeFile file: libs.utils.REVISION_VERSION_FILENAME, text: libs.utils.get_revision_version()
    }
    archiveArtifacts artifacts: "${libs.utils.OUTPUT_DIR_NAME}/*.*"
}

/**
 * Return proper template name for RAS US Local tests depends on the OS type.
 * @param job_params Params of the Jenkins job - with RAS_US_LOCAL_OS defined.
 * @return name of the template to run from AWX.
 */
def get_awx_template_name_for_ras_local(job_params) {
    if (job_params.RAS_US_LOCAL_OS == "linux") {
        return "runRasUnsafeShutdownLocalLinux"
    }
    else {
        return "runRasUnsafeShutdownLocalWindows"
    }
}

/**
 * Return proper extra vars for RAS US Local template depends on the OS type.
 * @param job_params Params of the Jenkins job - with RAS_US_LOCAL_OS defined.
 * @return extra vars to pass to the template on AWX.
 */
def get_extra_variables_for_ras_local(job_params) {
    // extra_vars have to be in proper YAML format, so please don't rework indentation in the return string if not necessary
    if (job_params.RAS_US_LOCAL_OS == "linux") {
        // prepare variables for run of Ansible playbook
        def working_dir_path = "/opt/rasUnsafeShutdownLocal"
        def logs_dir_path = "${working_dir_path}${currentBuild.id}"
        return """
working_dir_path: "${working_dir_path}"
logs_dir_path: "${logs_dir_path}"
repo:
  pmdk:
    url: '${job_params.PMDK_REPO_URL}'
    branch: '${job_params.BRANCH}'
    target_dir: "${working_dir_path}/pmdk"
  pmdk_tests:
    url: https://github.com/pmem/pmdk-tests
    branch: '${job_params.BRANCH_TESTS}'
    target_dir: "${working_dir_path}/pmdk-tests"
        """
    }
    else {
        return """
logs_dir_path: 'c:\\\\rasUnsafeShutdown${currentBuild.id}'
pmdk_branch: '${job_params.BRANCH}'
pmdk_tests_branch: '${job_params.BRANCH_TESTS}'
pmdk_tests_url: https://github.com/pmem/pmdk-tests
pmdk_url: '${job_params.PMDK_REPO_URL}'
        """
    }
}

/**
 * Parse Ansible task printing clonned revisions. Add found revisions to known SHAs.
 * @param awx_response AWXJobOutput instance from RAS execution
 */
def gather_repos_sha(awx_response) {
    echo "Gather git revisions SHA"

    def sha_task_name = "Print revision SHA"
    def sha_task_output = awx_response.get_task_output(sha_task_name)
    if (sha_task_output == null) {
        echo "Cannot find task ${sha_task_name} - SHA could not be gathered."
        return
    }
    echo "SHA task content:\n${sha_task_output}"

    /* Exemplary regex input:
        ok: [localhost] => (item=) => {
            "msg": "https://github.com/pmem/pmdk.git f87b1899fb989144420b042acb75e0d40607161f"
        }
        ok: [localhost] => (item=) => {
            "msg": "https://github.com/pmem/pmdk-tests e6d4935aa9e95a05c37bb0e00cae25cd58470476"
        }
     */
    def pattern = /
        \"msg\":\s\"    # first find literally string '"msg": "'
        (?<url>         # beginning of 1st capturing group, named 'url'...
            \S+         # ...which will capture sequence of non-whitespace characters
        )               # end of 1st capturing group
        \s              # then should be single whitespace character
        (?<sha>         # beginning of 2nd capturing group, named 'sha'...
            \S+         # ...which will capture sequence of non-whitespace characters
        )               # end of 2nd capturing group
        \"              # then, at the end, should be double-quote character placed
    /

    def revisions = libs.utils.apply_regex(sha_task_output, pattern, ["url", "sha"])
    if(revisions.size() == 0) {
        echo "Cannot find any SHA inside task ${sha_task_name}. Its output:\n${sha_task_output}"
        return
    }

    revisions.each {
        echo "Found SHA: ${it.url}: ${it.sha}"
        libs.utils.remember_git_revision(it.url, it.sha)
    }
}

/**
 * Validate correctness of Ansible/AWX job with RAS execution.
 * If any errors found, mark Jenkins job as unstable.
 * @param awx_response AWXJobOutput instance from RAS execution
 * @param selected_nodes List of SimpleNode objects, holding all nodes selected for RAS execution.
 */
def validate_response(awx_response, selected_nodes) {
    if (awx_response == null) {
        error("AWX response is not set to a valid object, probably due to earlier failures.")
    }

    def error_list = []
    def report_error_message = {
        error_message ->
            error_list.add(error_message)
            echo error_message
    }
    // check job status:
    def job_status = awx_response.get_job_status()
    if (job_status["status"] != "successful") {
        report_error_message("Overall job status was '${job_status["status"]}', instead of successful.")
    }
    else {
        echo("Job status is successful. Ok.")
    }

    // check playbook recap:
    def play_recap = awx_response.get_recap_entries()
    if (play_recap.size() < selected_nodes.size()) {
        report_error_message("Ansible job was not ended with min. ${selected_nodes.size()} host recap(s)!\nFound play recaps:\n${play_recap}")
    }
    else {
        play_recap.each {
            echo "Validating recap for host: ${it["ip"]}"
            def unreachable = it["unreachable"].toInteger()
            if (unreachable > 0) {
                report_error_message("Unreachable tasks: ${unreachable}")
            }
            else {
                echo("There are no unreachable tasks. Ok.")
            }
            def failed = it["failed"].toInteger()
            if (failed > 0) {
                report_error_message("Failed tasks: ${failed}")
            }
            else {
                echo("There are no failed tasks. Ok.")
            }
        }
    }

    // task with the following name should be skipped on exactly one machine if everything went well
    // if something went wrong, it will contain list of errors on its body
    def found_errors_task_name = "Fail if found errors"
    def found_errors_task = awx_response.get_task_status(found_errors_task_name)
    if (found_errors_task.size() != 1) {
        report_error_message("Expected exactly one status of task '${found_errors_task_name}', found ${found_errors_task.size()} instead.")
    }
    else {
        if (found_errors_task[0]["status"] != "skipping") {
            def what_went_wrong = awx_response.get_task_output(found_errors_task_name)
            what_went_wrong = what_went_wrong.replaceAll(/\\\\n/, "\n") // replace each "\\n" with a newline
            report_error_message("Execution failed:\n${what_went_wrong}")
        }
        else {
            echo("Tests went without errors. Ok.")
        }
    }
    
    if (error_list.any()) {
        def error_message = "During tests execution not everything went as expected:"
        error_list.each { error_message += "\n${it}" }
        echo error_message
        currentBuild.result = "UNSTABLE"
    }
    else {
        echo("Ansible execution looks ok.")
    }
}

/**
 * Obtain from Ansible/AWX job output generated log files and DUT where logs are stored.
 * @param awx_response AWXJobOutput instance from RAS execution.
 * @param selected_nodes List of SimpleNode objects, holding all nodes selected for RAS execution.
 * @return dictionary object, containing:
 *  - "log_files": list of paths to XML log files, in case of failure in getting this list, empty list is returned
 *  - "node_with_logs": SimpleNode instance of DUT with logs, in case of failure in this node, null object is returned.
 */
def get_log_files_data(awx_response, selected_nodes) {
    def log_files = []
    def node_with_logs = null
    // task with the following name should be "ok" on exactly one machine (even if something went wrong)
    def log_files_task_name = "Print all created log files"
    def log_files_task_status = awx_response.get_task_status(log_files_task_name).find { it["status"] == "ok" }
    if (log_files_task_status == null) {
        echo("Task '${log_files_task_name}' has no 'ok' status!")
    }
    else {
        // find all log files generated by Ansible run:
        def log_files_output = awx_response.get_task_output(log_files_task_name)

        /* Example input:
          ok: [10.91.28.121] => {
             "found_log_files": [
                 "/root/rasUnsafeShutdown/logs/phase1.xml",
                 "/root/rasUnsafeShutdown/logs/phase2.xml"
             ]
         }

         */
        def log_files_extraction_pattern = $/
            ^\s*                            # at the beginning of line could be some whitespaces
                \"                          # then opening double quote '"'
                    (?<logPath>[^"]+)       # capture sequence of every character that isn't double quote '"'
                \"                          # then closing double quote '"'
                ,?                          # then could appear optional comma
            \s*$                            # and maybe some whitespaces to the end of line
        /$

        def all_found_log_paths = libs.utils.apply_regex(log_files_output, log_files_extraction_pattern, ["logPath"])
        all_found_log_paths.each { log_files.add(it["logPath"]) }
        def ip_dut_with_logs = log_files_task_status["ip"]
        node_with_logs = selected_nodes.find { it.ip == ip_dut_with_logs }
        echo("Log files on the DUT ${node_with_logs.label} / ${node_with_logs.ip}:")
        log_files.each { echo it }
    }

    def retval = [:]
    retval["log_files"] = log_files
    retval["node_with_logs"] = node_with_logs
    return retval
}

/**
 * Send e-mail with test results summary.
 * @param test_results List of test results
 * @param send_results Flag indicating if mail should be sent/
 */
def send_email(test_results, send_results) {
    if (send_results) {
        libs.utils.send_test_summary_via_mail(test_results)
        libs.utils.send_test_summary_to_webpage(test_results)
    }
}

return this