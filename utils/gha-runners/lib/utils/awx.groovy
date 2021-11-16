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

// This file contains API for running AWX template jobs.

// label of the Jenkins agent with preinstalled tower-cli
AGENT = 'slave-tower-cli'

// flag for checking if tower-cli was configured; don't set it outside this script
awx_was_configured = false

/**
 * Class describing AWX Job result.*/
class AwxJobResult {
    /**
     * Constructor. At the construction step all output parsing is done.
     * All passed output should be captured after the AWX job is completed.
     * @param stdout_output Output from `tower_cli job stdout ID` in the form of a string.
     * @param get_output Output from `tower_cli job get ID` in the form of a string.
     * @param apply_regex_context Context from which the `apply_regex` will be called (in this case it'll be `api`).
     * We can't assign the function via metaClass because `apply_regex` function is a NonCPS one.
     */
    AwxJobResult(stdout_output, get_output, apply_regex_context) {
        this.ansible_output = stdout_output
        this.tasks = apply_regex_context.apply_regex(this.ansible_output,
                                                     this.ansible_tasks_pattern,
                                                     ["taskName", "taskBody"])

        def play_recap = apply_regex_context.apply_regex(stdout_output,
                                                         this.play_recap_pattern,
                                                         ["playRecap"])

        if (play_recap.isEmpty()) {
            throw new Exception("AWX job stdout was not correct. Missing 'PLAY RECAP'!")
        }
        this.recap_entries = apply_regex_context.apply_regex(play_recap[0]["playRecap"],
                                                             this.recap_entry_pattern,
                                                             ["ip",
                                                              "ok",
                                                              "changed",
                                                              "unreachable",
                                                              "failed",
                                                              "skipped",
                                                              "rescued",
                                                              "ignored"])

        def job_summary = apply_regex_context.apply_regex(get_output,
                                                          this.job_summary_pattern,
                                                          ["jobStatus"])
        if (job_summary.isEmpty()) {
            this.job_status = ["status": "", "jobId": "", "jobTemplate": "", "creationDate": "", "elapsed": ""]
        }
        else {
            this.job_status = apply_regex_context.apply_regex(job_summary[0]["jobStatus"],
                                                              this.job_status_pattern,
                                                              ["status", "jobId", "jobTemplate", "creationDate", "elapsed"])[0]
        }

        this.task_statuses = []
        for (task in this.tasks) {
            def task_output = task["taskBody"]
            def task_status = [:]
            task_status["taskName"] = task["taskName"]
            task_status["taskStatus"] = apply_regex_context.apply_regex(task_output,
                                                                        this.task_status_pattern,
                                                                        ["ip", "status"])
            this.task_statuses.add(task_status)
        }
    }

    /**
     * Returns as a string whole output from Ansible playbook, cutting parts from tower_cli out.    */
    def get_ansible_output() {
        return this.ansible_output
    }

    /**
     * Returns job status summed up by the tower_cli.
     * The returned object is of type dictionary with the following keys: ["status", "jobId", "jobTemplate", "creationDate", "elapsed"].    */
    def get_job_status() {
        return this.job_status
    }

    /**
     * Returns Ansible's recap entries.
     * The returned object is of type list of dictionaries with the following keys: ["ip", "ok", "changed", "unreachable", "failed", "skipped", "rescued", "ignored"].    */
    def get_recap_entries() {
        return this.recap_entries
    }

    /**
     * Returns Ansible's output from specific task.
     * If requested task not found, returns null.
     * If more than one task have the requested name, returns first occurrence.
     * The returned object is of type string.
     * @param task_name Name of the task we want to get output from. It should be exact matched, no wildcards or regexes are supported.
     */
    def get_task_output(task_name) {
        def found_task = this.tasks.find { it["taskName"] == task_name }
        if (found_task == null) {
            return null;
        }
        return found_task["taskBody"]
    }

    /**
     * Returns list of executed Ansible's tasks.
     * The returned object is of type list of dictionaries with the following keys: ["taskName", "taskBody"].    */
    def get_tasks() {
        return this.tasks
    }

    /**
     * Returns Ansible's status from specific task.
     * If requested task not found, returns null.
     * If more than one task have the requested name, returns first occurrence.
     * The returned object is of type list of dictionaries  with the following keys: ["ip", "status"].
     * @param task_name Name of the task we want to get status from. It should be exact matched, no wildcards or regexes are supported.
     */
    def get_task_status(task_name) {
        def found_task_status = this.task_statuses.find { it["taskName"] == task_name }
        if (found_task_status == null) {
            return null;
        }
        return found_task_status["taskStatus"]
    }

    private final def recap_entries
    private final def job_status
    private final def ansible_output
    private final def tasks
    private final def task_statuses

    /**
     * Pattern for obtaining recap summary from Ansible.
     * Exemplary input:
     Current status: pending
     Current status: running
     ------Starting Standard Out Stream------
     SSH password:

     PLAY [10.91.28.117] ************************************************************

     TASK [Gathering Facts] *********************************************************
     ok: [10.91.28.117]

     TASK [Celanup: reboot machine in order to apply AppDirect goal] ****************

     changed: [10.91.28.117]

     PLAY RECAP *********************************************************************
     10.91.28.117               : ok=48   changed=25   unreachable=0    failed=0    skipped=10   rescued=0    ignored=0

     Resource changed.
     */
    private final def play_recap_pattern = $/
        ^\s*PLAY\sRECAP\s\*+\n                                      # find line with "PLAY RECAP ******"
        (?<playRecap>[\s\S]*)                                       # first capture group: capture all lines between that "PLAY RECAP" line and... 
    /$

    /**
     * Pattern for obtaining job summary from tower_cli.
     * Exemplary input:
     ------Starting Standard Out Stream------
     ------End of Standard Out Stream--------
     === ============ =========================== ========== =======
     id  job_template           created             status   elapsed
     === ============ =========================== ========== =======
     266           34 2020-04-22T09:29:53.771404Z successful 460.953
     === ============ =========================== ========== =======

     */
    private final def job_summary_pattern = $/
        ^\s*id\s+job_template\s+created\s+status\s+elapsed\s*\n     # find line with "id job_template created status elapsed"... 
        ^[\s=]+$                                                    # ...followed by a border line with only whitespaces and '=' characters
        (?<jobStatus>[\s\S]*)                                       # second capture group: capture all lines between that border line and...
        ^[\s=]+$                                                    # ...another border line only whitespaces and '=' characters 
    /$

    /**
     * Pattern for obtaining Ansible's recap entries.
     * Exemplary input:
     10.91.28.117               : ok=48   changed=25   unreachable=0    failed=0    skipped=10   rescued=0    ignored=0

     10.91.28.119               : ok=47   changed=25   unreachable=0    failed=0    skipped=11   rescued=0    ignored=0

     */
    private final def recap_entry_pattern = $/
        ^                           # match starts at the beginning of the line...
            \s*?(?<ip>\S+)          # then could be whitespaces, followed by sequence of non-white-characters - host IP, 1. capturing group
            \s*?:                   # then could be whitespaces followed by a colon ':'
            \s*ok=                  # then could be whitespaces, followed by "ok=" string
            (?<ok>\d+)              # sequence of digits - 2. capturing group, number of OK tasks
            \s*changed=             # then could be whitespaces, followed by "changed=" string
            (?<changed>\d+)         # sequence of digits - 3. capturing group, number of changed tasks
            \s*unreachable=         # then could be whitespaces, followed by "unreachable=" string
            (?<unreachable>\d+)     # sequence of digits - 4. capturing group, number of unreachable tasks
            \s*failed=              # then could be whitespaces, followed by "failed=" string
            (?<failed>\d+)          # sequence of digits - 5. capturing group, number of failed tasks
            \s*skipped=             # then could be whitespaces, followed by "skipped=" string
            (?<skipped>\d+)         # sequence of digits - 6. capturing group, number of skipped tasks
            \s*rescued=             # then could be whitespaces, followed by "rescued=" string
            (?<rescued>\d+)         # sequence of digits - 7. capturing group, number of rescued tasks
            \s*ignored=             # then could be whitespaces, followed by "ignored=" string
            (?<ignored>\d+)         # sequence of digits - 8. capturing group, number of ignored tasks
        \s*$                        # then could appear some whitespaces and end of the line
    /$

    /**
     * Pattern for obtaining recap summary from tower_cli.
     * Exemplary input:
     266           34 2020-04-22T09:29:53.771404Z successful 460.953

     */
    private final def job_status_pattern = $/
        ^                               # match starts at the beginning of the line...
            \s*(?<jobId>\d+)            # possible whitespaces and 1. capturing group: sequence of digits as job ID
            \s*(?<jobTemplate>\d+)      # possible whitespaces and 2. capturing group: sequence of digits as job template ID
            \s*(?<creationDate>\S+)     # possible whitespaces and 3. capturing group: sequence of non-whitespace chars as creation date
            \s*(?<status>\S+)           # possible whitespaces and 4. capturing group: sequence of non-whitespace chars as job status
            \s*(?<elapsed>\S+)          # possible whitespaces and 5. capturing group: sequence of non-whitespace chars as elapsed time
        \s*$                            # then could appear some whitespaces and end of the line
    /$

    /**
     * Pattern for obtaining tasks: name and body, based on Ansible output.
     * Exemplary input:
     SSH password:

     PLAY [10.91.28.117] ************************************************************

     TASK [Gathering Facts] *********************************************************
     ok: [10.91.28.117]

     TASK [Celanup: reboot machine in order to apply AppDirect goal] ****************

     changed: [10.91.28.117]

     PLAY RECAP *********************************************************************
     10.91.28.117               : ok=48   changed=25   unreachable=0    failed=0    skipped=10   rescued=0    ignored=0

     */
    private final def ansible_tasks_pattern = $/
        TASK\s+\[(?<taskName>[\s\S]+?)\]\s+\*+      # capture task name from line "TASK [task name] ***"
        (?<taskBody>                                # capture task body:
            (?:                                     # non-capturing group for apply "one or more" to 2 regex statements
                [\s\S]                              # any character...
                (?!TASK\s\[|PLAY\sRECAP)            # but not next TASK or PLAY RECAP; negative lookahead group in order to don't consume beginning of the next TASK
            )+                                      # close non-capturing group
        )                                           # close capturing group
    /$

    /**
     * Pattern for obtaining task status, based on the task output.
     * Exemplary input:
     changed: [10.91.28.117]
     ok: [10.91.28.119]

     */
    private final def task_status_pattern = $/
        ^\s*                # some possible whitespaces at the beginning of the line
        (?<status>\S+)      # then status string - first capturing group
        :\s+\[              # then ": [" string
        (?<ip>\S+)          # then IP - second capturing group
        \]                  # then closing bracket "]"
    /$
}
this.AwxJobResult = AwxJobResult

/**
 * Do necessary tower-cli configuration before running any job template.
 * Credentials used: 'awx-admin' - should be configured in credentials on Jenkins instance.
 * @param awx_url URL of AWX instance.
 */
def configure_tower_instance(awx_url) {
    withCredentials([usernamePassword(credentialsId: 'awx-admin', passwordVariable: 'awxPwd', usernameVariable: 'awxUsr')]) {
        sh "tower-cli config host $awx_url"
        sh "tower-cli config username $awxUsr"
        sh "tower-cli config password $awxPwd"
        sh "tower-cli config verify_ssl False"
    }
    awx_was_configured = true
}

/**
 * Helper function which throws when tower-cli was not configured yet.*/
def ensure_awx_was_configured(function_name) {
    if (!awx_was_configured) {
        throw new Exception("AWX was not configured, but function '${function_name}' using tower-cli was called! Please run 'configure_tower_instance' function first!")
    }
}

/**
 * Obtain from tower-cli ID of template job with given name and from given project.
 * Throws exception when requested template job name or project name not found.
 * @param template_name Name of the template job to run. E.g. `setProxy` or `runPmbench`.
 * @param project_name Name of the project where the job template will be searched.
 * @return Job template ID in the form of string.
 */
def get_template_id(String template_name, String project_name = "pmdk_files") {
    ensure_awx_was_configured("get_template_id")
    def project_list = sh(script: "tower-cli project list --all-pages", returnStdout: true)
    echo "project list:\n${project_list}"

    def template_list = sh(script: "tower-cli job_template list --all-pages", returnStdout: true)
    echo "job_template list:\n${template_list}"

    /*
     * Pattern for parsing `tower-cli project list` output.
     * Exemplary input:
     == ========== ======== ===================================================== ==============
     id    name    scm_type                        scm_url                          local_path
     == ========== ======== ===================================================== ==============
     8 pmdk_files git      https://gitlab.devtools.intel.com/pmdk/pmdk_files.git _8__pmdk_files
     31 ras        git      https://gitlab.devtools.intel.com/pmdk/pmdk_files.git _31__ras
     == ========== ======== ===================================================== ==============

     */
    def project_pattern = $/
        ^                       # match starts at the beginning of the line...
            \s*(?<id>\d+)       # then could be whitespaces, followed by sequence of digits - project ID, first capturing group
            \s*(?<name>\S+)     # then some whitespaces, followed by sequence of non-white-chars - project name, second capturing group
            \s*git              # then literally word 'git' preceeded by some whitespaces - scm type (its always git for us)
            \s*\S+              # then some whitespaces, followed by sequence of non-white-chars - scm URL
            \s*\S+              # then some whitespaces, followed by sequence of non-white-chars - local path
        \s*$                    # then could appear some whitespaces and end of the line
    /$

    def parsed_projects = libs.utils.apply_regex(project_list, project_pattern, ["id", "name"])
    if (parsed_projects.isEmpty()) {
        throw new Exception("Cannot parse `tower-cli project list` output!")
    }

    def requested_project = parsed_projects.find { it["name"] == project_name }
    if (requested_project == null) {
        throw new Exception("Requested '${project_name}' project not found!")
    }
    def requested_project_id = requested_project["id"]

    // name of the job template is concatenated with underscore template name and project name (in our autogenerated environment):
    def requested_name = template_name + "_" + project_name

    /*
     * Pattern for parsing `tower-cli job_template list` output.
     * Exemplary input:
     == ============================================= ========= ======= =============================================================
     id                     name                      inventory project                           playbook
     == ============================================= ========= ======= =============================================================
     12 configureEnv_pmdk_files                               2       8 ansible/main/playbooks/configureEnv.yml
     34 runRasLocal_ras                                       3      31 ansible/main/playbooks/runRasUnsafeShutdownLocalLinux.yml
     26 setProxy_pmdk_files                                   2       8 ansible/main/playbooks/setProxy.yml
     35 setProxy_ras                                          3      31 ansible/main/playbooks/setProxy.yml
     == ============================================= ========= ======= =============================================================

     */
    def template_pattern = $/
        ^                               # match starts at the beginning of the line...
            \s*(?<templateId>\d+)       # then could be whitespaces, followed by sequence of digits - template ID, 1. capturing group
            \s*(?<templateName>\S+)     # then some whitespaces, followed by sequence of non-white-chars - template name, 2. capturing group
            \s*(?<inventoryId>\d+)      # then some whitespaces, followed by sequence of digits - inventory ID, 3. capturing group
            \s*(?<projectId>\d+)        # then some whitespaces, followed by sequence of digits - project ID, 4. capturing group
            \s*(?<playbookName>\S*)     # then some whitespaces, followed by sequence of non-white-chars - playbook name, 5. capturing group
        \s*$                            # then could appear some whitespaces and end of the line
    /$

    def parsed_templates = libs.utils.apply_regex(template_list, template_pattern, ["templateName", "projectId", "templateId"])
    def requested_template = parsed_templates.find {
        it["templateName"] == requested_name && it["projectId"] == requested_project_id
    }
    if (requested_template == null) {
        throw new Exception("Requested template '${requested_name}' not found within project '${project_name}'.")
    }
    return requested_template["templateId"]
}

/**
 * Run job on AWX using tower-cli. This function will not monitor the running job.
 * In case of bad tower-cli response, throws an exception.
 * @param job_template_name Name of the job template to run.
 * @param host String which will be passed to `host` variable.
 * @param extra_vars Additional extra vars. Remember to keep proper YAML format! Default empty.
 * @param project Project in which the job template will be searched. Default is "pmdk_files".
 * @return Executed job ID in the form of string.
 */
def run_job(String job_template_name, String host, String extra_vars = "", String project = "pmdk_files") {
    ensure_awx_was_configured("run_job")
    def template_id = get_template_id(job_template_name, project)
    def all_vars = "---\nhost: ${host}\n${extra_vars}\n"
    def tower_cli_output = libs.os.run_script("tower-cli job launch --job-template=${template_id} --extra-vars='${all_vars}'",
                                                   "job_launch.log").output

    /** Pattern for parsing `tower-cli job launch` output.
     * Exemplary input:
     Resource changed.
     == ============ =========================== ======= =======
     id job_template           created           status  elapsed
     == ============ =========================== ======= =======
     38           26 2020-04-17T12:41:54.764743Z pending 0.0
     == ============ =========================== ======= =======

     */
    def job_pattern = $/
        ^                               # match starts at the beginning of the line...
            \s*(?<jobId>\d+)            # then could be whitespaces, followed by sequence of digits - job ID, 1. capturing group
            \s*(?<jobTemplate>\d+)      # then some whitespaces, followed by sequence of digits - job template ID, 2. capturing group
            \s*(?<creationDate>\S+)     # then some whitespaces, followed by sequence of non-white-chars - creation date, 3. capturing group
            \s*(?<status>\S+)           # then some whitespaces, followed by sequence of non-white-chars - job status, 4. capturing group
            \s*(?<elapsed>\S+)          # then some whitespaces, followed by sequence of non-white-chars - time elapsed since start, 5. capturing group
        \s*$                            # then could appear some whitespaces and end of the line
    /$
    def parsed_jobs = libs.utils.apply_regex(tower_cli_output, job_pattern, ["jobTemplate", "jobId"])
    def requested_job = parsed_jobs.find { it["jobTemplate"] == template_id }
    if (requested_job == null) {
        throw new Exception("Started job ID not found! tower-cli output:\n${tower_cli_output}")
    }
    return requested_job["jobId"]
}

/**
 * Run job on AWX using tower-cli. This function will monitor the running job and return {@link AwxJobResult AWX-output-object}.
 * @param job_template_name Name of the job template to run.
 * @param host String which will be passed to `host` variable.
 * @param extra_vars Additional extra vars. Remember to keep proper YAML format! Default empty.
 * @param project Project in which the job template will be searched. Default is "pmdk_files".
 * @return Executed job output in the form of {@link AwxJobResult AWX-output-object}.
 */
def run_job_and_monitor(String job_template_name, String host, String extra_vars = "", String project = "pmdk_files") {
    ensure_awx_was_configured("run_job_and_monitor")
    def job_id = run_job(job_template_name, host, extra_vars, project)
    def job_monitor_live_filename = "job.monitor.live.output.log"
    def job_stdout_filename = "job.stdout.log"
    def job_get_filename = "job.get.log"
    def job_stdout_content = ""
    def job_get_content = ""
    try {
        libs.os.rm(job_monitor_live_filename)
        libs.os.rm(job_stdout_filename)
        libs.os.rm(job_get_filename)
        echo "Live monitor AWX job"
        libs.os.run_script("tower-cli job monitor ${job_id}", job_monitor_live_filename)
    } catch (Exception ex) {
        // suppress all exceptions from running `tower-cli job monitor` command
    } finally {
        echo "Retrieve output and status from completed AWX job"
        libs.os.run_script("tower-cli job stdout ${job_id}", job_stdout_filename, false)
        libs.os.run_script("tower-cli job get ${job_id}", job_get_filename, false)
        job_stdout_content = readFile job_stdout_filename
        job_get_content = readFile job_get_filename
    }
    return new AwxJobResult(job_stdout_content, job_get_content, libs.utils)
}

// below is necessary in order to closure work properly after loading the file in the pipeline:
return this
