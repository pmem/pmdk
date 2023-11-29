# Persistent Memory Development Kit

This is utils/ansible/README.md.

The scripts in this directory allow you to set up a RockyLinux and OpenSUSE
environment on a real hardware to be able build a PMDK project on it or use the
node as a self-hosted runner for pmem/pmdk repository.

# Installing Ansible
To use playbooks it is required to install Ansible first. It must be done
on a computer that will be used to execute the script, not necessarily
on the target platform.

Please check ansible documentation on available installation methods.
https://docs.ansible.com/ansible/latest/installation_guide/intro_installation.html

Alternatively, install it from ready to use packages as shown below:
```sh
sudo dnf install ansible-core
# or
sudo zypper install ansible
# or
sudo apt install ansible-core
```

Additionally, install the ansible.posix module to be able to run
the configure-pmem.yml and rockylinux-setup.yml scripts
```sh
sudo ansible-galaxy collection install ansible.posix
```

# Provisioning basics
## Provisioning the target platform
Use the command below to setup the PMDK software development environment:
```sh
export TARGET_IP= # ip of the target
export ROOT_PASSWORD= # a password of root on the target
export SETUP_SCRIPT= # opensuse-setup.yml or rockylinux-setup.yml

ansible-playbook -i $TARGET_IP, $SETUP_SCRIPT \
  --extra-vars "host=all ansible_user=root ansible_password=$ROOT_PASSWORD \
  testUser=pmdkuser testUserPass=pmdkpass"
```
**Note**: If the Linux kernel is outdated, `opensuse-setup.yml` and
`rockylinux-setup.yml` playbooks will reboot the target platform.

## Provisioning persistent memory
Use the below command to configure persistent memory on Intel servers to be
used for PMDK libraries tests execution.
```sh
export TARGET_IP= # ip of the target
export ROOT_PASSWORD= # a password of root on the target
ansible-playbook -i $TARGET_IP, configure-pmem.yml \
  --extra-vars "host=all ansible_user=root ansible_password=$ROOT_PASSWORD \
  newRegions=true testUser=pmdkuser"
```
The script creates a new region and set of namespaces regardless of the
configuration already available on the target platform.

**Note**: Every time when new region is created (`newRegions=true`) the playbook
will reboot the target platform.

**Note**: The given above command can be used whenever full reinitialization
of persistent memory is required.

**Note**: Configured regions are required to create namespaces on the DIMMs.

### Namespace re-initialization
The following command will delete all existing namespaces and create new ones.
It can be used to perform a full memory cleanup as part of a reinitialization of
the test environment.

**Note**: It is NOT required to re-create regions with the `newRegions=true`
parameter in this case.
```sh
export TARGET_IP= # ip of the target
export ROOT_PASSWORD= # a password of root on the target
ansible-playbook -i $TARGET_IP, configure-pmem.yml \
  --extra-vars "host=all ansible_user=root ansible_password=$ROOT_PASSWORD \
  testUser=pmdkuser"
```

# Installing a GitHub Action self-hosted runner using Ansible palybook
The sequence of commands below setup a new server, with persistent memory and
a CI environment already installed, to be used as a self-hosted runner in
the pmem/pmdk repository.

```sh
export TARGET_IP= # ip of the target
export ROOT_PASSWORD= # a password of root on the target
export GHA_TOKEN= # GitHub token generated for a new self-hosted runner
export HOST_NAME= # host's name that will be visible on GitHub
export LABELS= # rhel or opensuse
export VARS_GHA= # e.g. proxy settings: http_proxy=http://proxy-dmz.{XXX}.com:911,https_proxy=http://proxy-dmz.{XXX}.com:912
ansible-playbook -i $TARGET_IP, configure-self-hosted-runner.yml --extra-vars
  "host=all ansible_user=root ansible_password=$ROOT_PASSWORD testUser=pmdkuser \
  runner_name=$HOST_NAME labels=$LABELS token=$GHA_TOKEN vars_gha=$VARS_GHA"
```
**Note**: To obtain a token for a new self hosted runer visit
[Create self-hosted runner](https://gib.com/pmem/pmdk/settings/actions/runners/new)
.

**Note**: In case of any problems, please refer to
[Adding self-hosted runners](https://docs.github.com/en/actions/hosting-your-own-runners/managing-self-hosted-runners/adding-self-hosted-runners) documentation.

# Provisioning from the target platform itself
It is possible to run playbooks directly on the target platform.
To run playbooks inside the platform please comment out the line:
```
- hosts: "{{ host }}"
```
uncomment the following two:
```
# - hosts: localhost
#   connection: local
```
and run commands as follows e.g.
```sh
export SETUP_SCRIPT= # opensuse-setup.yml or rockylinux-setup.yml
sudo ansible-playbook $SETUP_SCRIPT --extra-vars "testUser=pmdkuser"
```
**Note**: If a reboot is necessary, as described above, perform it manually and
rerun the playbook withot  in question.

And next:
```sh
sudo ansible-playbook ./configure-pmem.yml --extra-vars "testUser=pmdkuser newRegions=true"
# you will have to perform a reboot manually
reboot
# and re-run the playbook without newRegions=true to finalize the setup
sudo ansible-playbook ./configure-pmem.yml --extra-vars "testUser=pmdkuser"
```

# Example - GitHub self-hosted runner setup
The sequence of commands below presents an example procedure for how to setup
a new server, with the base OS already installed, to be used as a self-hosted
GHA runner without a control node.

## Provisioning the platform and persistent memory
### For RockyLinux
```sh
# as root:
dnf install git-core -y
dnf install ansible-core -y
ansible-galaxy collection install ansible.posix
git clone https://github.com/pmem/pmdk.git
cd pmdk/utils/ansible
```
Update playbooks to be used directly on the target as described [above](#provisioning-from-the-target-platform-itself)
and execute:
```sh
# as root:
ansible-playbook rockylinux-setup.yml --extra-vars "testUser=pmdkuser testUserPass=pmdkpass"
# reboot shall be performed only if the playbook requests to do it.
reboot
# ...
cd pmdk/utils/ansible
ansible-playbook rockylinux-setup.yml --extra-vars "testUser=pmdkuser testUserPass=pmdkpass"
ansible-playbook configure-pmem.yml --extra-vars "newRegions=true"
reboot
# ...
cd pmdk/utils/ansible
# note - no newRegions=true when running the playbook after the reboot
ansible-playbook configure-pmem.yml --extra-vars "testUser=pmdkuser"

export GHA_TOKEN= # GitHub token generated for a new self-hosted runner
export HOST_NAME=`hostname`
export LABELS= rhel
export VARS_GHA=http_proxy=http://proxy-dmz.{XXX}.com:911,https_proxy=http://proxy-dmz.{XXX}.com:912
ansible-playbook configure-self-hosted-runner.yml -extra-vars \
"testUser=pmdkuser runner_name=$HOST_NAME labels=$LABELS token=$GHA_TOKEN vars_gha=$VARS_GHA"
cd
rm -rf pmdk
```

### For OpenSUSE
```sh
# as root:
zypper install git-core -y
zypper install ansible -y
git clone https://github.com/pmem/pmdk.git
cd pmdk/utils/ansible
```
Update playbooks to be used directly on the target as described [above](#provisioning-from-the-target-platform-itself)
and execute:
```sh
# as root:
ansible-playbook ./opensuse-setup.yml --extra-vars "testUser=pmdkuser testUserPass=pmdkpass"
# reboot shall be performed only if the playbook requests to do it.
reboot
# ...
cd pmdk/utils/ansible
ansible-playbook ./opensuse-setup.yml --extra-vars "testUser=pmdkuser testUserPass=pmdkpass"
ansible-playbook ./configure-pmem.yml --extra-vars "newRegions=true"
reboot
# ...
cd pmdk/utils/ansible
# note - no newRegions=true when running the playbook after the reboot
ansible-playbook ./configure-pmem.yml --extra-vars "testUser=pmdkuser"

export GHA_TOKEN= # GitHub token generated for a new self-hosted runner
export HOST_NAME=`hostname`
export LABELS= opensuse
export VARS_GHA=http_proxy=http://proxy-dmz.{XXX}.com:911,https_proxy=http://proxy-dmz.{XXX}.com:912
ansible-playbook configure-self-hosted-runner.yml -extra-vars \
"testUser=pmdkuser runner_name=$HOST_NAME labels=$LABELS token=$GHA_TOKEN vars_gha=$VARS_GHA"
cd
rm -rf pmdk
```

