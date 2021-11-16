//
// Copyright 2021, Intel Corporation
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

// This file contains code for pipelines using restore snaphosts.

/**
 * Function will run timeshift app on a remote server, using sshagent plugin.
 * @param ssh_credentials Specified ssh credentials. Must be set on Jenkins first.
 * @param user_name User that ssh will try to log on.
 * @param ssh_address IP address of remote machine.
 * @param snapshot_name Specified name of a Timeshift snapshot.
 */
def restore_snaphot(ssh_credentials, user_name, ssh_address, snapshot_name) {
/*
	Expecting to lose connection, ServerAliveInterval=2 and ServerAliveCountMax=2
	will cause to exit script when connection is lost. 
	"printf '\ny\n0\ny\n'" - this simulates user imput necessary to use Timeshift.
*/
	sshagent (credentials: [ssh_credentials]) {
		libs.linux.run_bash_script("""
			ssh -o ServerAliveInterval=2 -o ServerAliveCountMax=2 -l ${user_name} ${ssh_address} "printf '\ny\n0\ny\n' | sudo timeshift --restore --snapshot ${snapshot_name}" || true 
		""")
	}
}

/**
 * Function will test and wait for connection to remote server.
 * @param ssh_credentials Specified ssh credentials. Must be set on Jenkins first.
 * @param user_name User that ssh will try to log on.
 * @param ssh_address IP address of remote machine.
 * @param no_of_tries Number of reconnect tries.
 * @param ConTimeout value of ssh ConnectTimeout parameter.
 * @param ConAttempts value of ssh ConnectionAttempts parameter.
 */
def wait_for_reconnect(ssh_credentials, user_name, ssh_address, no_of_tries = 200, ConTimeout = 1, ConAttempts = 1) {
	sshagent (credentials: [ssh_credentials]) {
		libs.linux.run_bash_script("""
			function wait_for_connection() {
				ssh -o ConnectTimeout=${ConTimeout} -o ConnectionAttempts=${ConAttempts} -o StrictHostKeyChecking=no -l ${user_name} ${ssh_address} 'true'
				if [ "\$?" -eq "0" ]; then
					echo 0
				else
					echo 1
				fi 
			}
			for (( i=${no_of_tries}; i>0; i-- ))
			do
				echo "trying to reach ${ssh_address}. \$i attempts left"
				if [ "\$(wait_for_connection)" -eq 0 ]; then
					break
				elif [ "\$i" -eq "1" ]; then
					echo "ERROR: Could not reach ${ssh_address}"
					exit 1
				fi
				sleep 1
			done
			sleep 10
		""")
	}
}

/**
 * Reconnect and unset online specified jenkins node.
 * @param node_name Jenkins node name.
 */
def reconnect_node(node_name) {
	for (jenkins_node in jenkins.model.Jenkins.instance.slaves) {
		println(jenkins_node.name);
		 if (jenkins_node.name == node_name) {
			println('Bringing node online.');
			jenkins_node.getComputer().connect(true)
			jenkins_node.getComputer().setTemporarilyOffline(false);
			break; 
		}
	}
}

/**
 * Disconnect and set offline specified jenkins node.
 * @param node_name Jenkins node name.
 */
def diconnect_node(node_name) {
	for (jenkins_node in jenkins.model.Jenkins.instance.slaves) {
		 if (jenkins_node.name == node_name) {
			println('Disconnecting node')
			jenkins_node.getComputer().disconnect(new hudson.slaves.OfflineCause.ByCLI("Disconnected by Jenkins"));
		}
	}
}

/**
 * Returns Jenkins node ip address.
 * @param node_name Jenkins node name.
 * @return Jenkins node ip address.
 */
def get_node_address(node_name) {
	for (jenkins_node in jenkins.model.Jenkins.instance.slaves) {
		 if (jenkins_node.name == node_name) {
			targetAdress = (jenkins_node.getLauncher().getHost())
			return targetAdress
		}
	}
}

/**
 * Returns Timeshift snapshot name from the server.
 * @param snapshot_option Option on which to choose proper Timeshift snapshot.
 	SKIP: skip selecting snapshot name
 	JENKINS_LATEST: A snapshot with description 'Jenkins_backup_latest'
 	will be searched on the target server and its name will be chosen.
 	LATEST: Latest snapshot on the target server will be selected.
 	CHOOSE: A specified snapshot name is chosed, passed in the 'snapshot_option_name'.
 * @param snapshot_option_name If 'CHOOSE' option is selected then the string
 	in the snapshot_option_name will be choosen as snapshot name.
 * @return Snapshot name.
 */
def get_snapshot_name(snapshot_option, snapshot_option_name='' ) {
	def snapshot_name = ''
	switch(snapshot_option) {
		case 'SKIP':
			snapshot_name = 'SKIP'
			break;
		case 'JENKINS_LATEST':
			snapshot_name = libs.linux.run_bash_script("""
				res=\$(sudo timeshift --list-snapshots | grep Jenkins_backup_latest)
				echo \$res | awk '{print \$3}' | head -n 1
			""").output
			break;
		case 'LATEST':
			snapshot_name = libs.linux.run_bash_script("""
				res=\$(sudo timeshift --list-snapshots | tail -n 2)
				echo \$res | awk '{print \$3}'
			""").output
		break;
		case 'CHOOSE':
			snapshot_name = snapshot_option_name
			break;
	}
	return snapshot_name
}
// Note: "Return this" is crucial.
return this
