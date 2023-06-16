# Persistent Memory Development Kit

This is utils/ansible/README.md.

The scripts in this directory allow you to set up a RockyLinux and OpenSUSE
environment on a real hardware and build a PMDK project on it.

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
# Provisioning basics
## Provisioning the target platform
Use the command below to setup the PMDK software development environment:
```sh
export TARGET_IP= # ip of the platform
export ROOT_PASSWORD= # password for root on target
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
export TARGET_IP= # ip of the platform
export ROOT_PASSWORD= # password for root on target
ansible-playbook -i $TARGET_IP, configure-pmem.yml \
  --extra-vars "host=all ansible_user=root ansible_password=$ROOT_PASSWORD \
  newRegions=true testUser=pmdkuser"
```
The script creates new region and set of namespace, regardles what configuration
has already been available on the target platform.

**Note**: Every time when new region is created (`newRegions=true`) the playbook
will reboot the target platform.

**Note**: The given above command can be used whenever full reinitialization
of persistent memory is required.

**Note**: Configured regions are required to create namespaces on the DIMMs.

### Namespace re-initialization
The following command will delete all existing namespaces and create new ones.
It can be used to perform a full memory cleanup as part of a reinitialization of
the test environment.

**Note** It is not required to re-create regions with the `newRegions=true`
parameter in this case. 
```sh
export TARGET_IP= # ip of the platform
export ROOT_PASSWORD= # password for root on target
ansible-playbook -i $TARGET_IP, configure-pmem.yml \
  --extra-vars "host=all ansible_user=root ansible_password=$ROOT_PASSWORD \
  testUser=pmdkuser"
```

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
sudo ansible-playbook opensuse-setup.yml --extra-vars "testUser=pmdkuser"`
# or
sudo ansible-playbook rockylinux-setup.yml --extra-vars "testUser=pmdkuser"`
```
and next:
```sh
sudo ansible-playbook ./configure-pmem.yml --extra-vars "testUser=pmdkuser newRegions=true"
```
**Note**: If a reboot is necessary, as described above, perform it manually and
rerun the playbook in question.

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
git clone https://github.com/pmem/pmdk.git
cd pmdk/utils/ansible
ansible-playbook ./rockylinux-setup.yml --extra-vars "testUser=pmdkuser testUserPass=pmdkpass"
reboot
# reboot shall be performed only if playbook requests to do it.
...
cd pmdk/utils/ansible
ansible-playbook ./rockylinux-setup.yml --extra-vars "testUser=pmdkuser testUserPass=pmdkpass"
ansible-playbook ./configure-pmem.yml --extra-vars "newRegions=true"
reboot
...
cd pmdk/utils/ansible
ansible-playbook ./configure-pmem.yml --extra-vars "testUser=pmdkuser"
# note - no newRegions=true when running the playbook after the reboot
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
ansible-playbook ./opensuse-setup.yml --extra-vars "testUser=pmdkuser testUserPass=pmdkpass"
reboot
# reboot shall be performed only if playbook requests to do it.
...
cd pmdk/utils/ansible
ansible-playbook ./opensuse-setup.yml --extra-vars "testUser=pmdkuser testUserPass=pmdkpass"
ansible-playbook ./configure-pmem.yml --extra-vars "newRegions=true"
reboot
...
cd pmdk/utils/ansible
ansible-playbook ./configure-pmem.yml --extra-vars "testUser=pmdkuser"
# note - no newRegions=true when running the playbook after the reboot
cd
rm -rf pmdk
```

## Installing and setting up a GitHub Actions runner
Installation and configuration of a self-hosted runner (as pmdkuser):
```sh
mkdir actions-runner && cd actions-runner
curl -o actions-runner-linux-x64-2.304.0.tar.gz -L \
https://github.com/actions/runner/releases/download/v2.304.0/actions-runner-linux-x64-2.304.0.tar.gz
tar xzf ./actions-runner-linux-x64-2.304.0.tar.gz
./config.cmd --url https://github.com/pmem/pmdk --token T_O_K_E_N__F_R_O_M__G_I_T_H_U_B
tmux
./run.cmd
```
Close session leaving the tmux session in the background (`CTRL+b d`).
