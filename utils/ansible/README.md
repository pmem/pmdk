# Persistent Memory Development Kit

This is utils/ansible/README.md.

The scripts in this directory allow you to set up a RockyLinux and OpenSUSE
environment on a real hardware and build a PMDK project on it.

# Installing Ansible
To use playbooks it is required to install Ansible first. It must be done
on a computer that will be used to execute the script, not necessarily
on the target platform.

Please check \[ansible documentation\]
(https://docs.ansible.com/ansible/latest/installation\_guide/intro\_installation.html#installing-and-upgrading-ansible)
on available installation methods.

Alternatively, install it from ready to use pacages as shown below:
```sh
dnf install ansible-core
# or
zypper install ansible
```
# Provisioning basics
## Provisioning the target platform
Use the commands below to setup the PMDK software development environment:
```sh
ansible-playbook -i <target_platform_ip_address>, [opensuse|rockylinux]-setup.yml --extra-vars \
 "host=all ansible_user=root ansible_password=<password_for_root_on_target> testUser=pmdkuser testUserPass=pmdkpass"
```
**Note**: If the Linux kernel is outdated, `opensuse-setup.yml` and
`rockylinux-setup.yml` playbooks will reboot the target platform.

## Provisioning PMem
Use the below commands to configure persistent memory on Intel servers
(regions and namespaces on DIMMs) to be used for PMDK libraries tests execution.

The `newRegions=true` option shall be used if the script is used for the very
first time on a given server to create new regions.

**Note**: Configured regions are required to create namespaces on the DIMMs.
```sh
ansible-playbook -i <target_platform_ip_address>, configure-pmem.yml --extra-vars \
 "host=all ansible_user=root ansible_password=<password_for_root> newRegions=true"
```
This will reboot the platform.

The following command will remove any existing namespaces and will create
new ones.
```sh
ansible-playbook -i <target_platform_ip_address>, configure-pmem.yml --extra-vars
 "host=all ansible_user=root ansible_password=<password_for_root_on_target> testUser=pmdkuser"
```

# Provisioning from the target node itself
It is possible to run playbooks inside the target platform. If a reboot will be
necessary, as described above, then you need to execute it manualy and rerun
a given playbook.

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
```

# Example - GitHub self-hosted runner setup
The sequence of commands below presents an example procedure for how to setup
a new server, with the based OS already installed, to be used as a self-hosted GHA
runner without a control node.

## Provisioning the platform and PMem
### For RockyLinux
As root:
```sh
dnf install git-core -y
dnf install ansible-core -y
git clone https://github.com/pmem/pmdk.git
cd pmdk/utils/ansible
ansible-playbook ./rockylinux-setup.yml --extra-vars "testUser=pmdkuser testUserPass=pmdkpass"
reboot
...
cd pmdk/utils/ansible
ansible-playbook ./rockylinux-setup.yml --extra-vars "testUser=pmdkuser testUserPass=pmdkpass"
ansible-playbook ./configure-pmem.yml --extra-vars "testUser=pmdkuser newRegions=true"
reboot
...
cd pmdk/utils/ansible
ansible-playbook ./configure-pmem.yml --extra-vars "testUser=pmdkuser"
# note no newRegions=true when running the playbook after the reboot
cd
rm -rf pmdk
```
### For OpenSUSE
As root:
```sh
zypper install git-core -y
zypper install ansible -y
git clone https://github.com/pmem/pmdk.git
cd pmdk/utils/ansible
ansible-playbook ./opensuse-setup.yml --extra-vars "testUser=pmdkuser testUserPass=pmdkpass"
reboot
...
cd pmdk/utils/ansible
ansible-playbook ./opensuse-setup.yml --extra-vars "testUser=pmdkuser testUserPass=pmdkpass"
ansible-playbook ./configure-pmem.yml --extra-vars "testUser=pmdkuser newRegions=true"
reboot
...
cd pmdk/utils/ansible
ansible-playbook ./configure-pmem.yml --extra-vars "testUser=pmdkuser"
# note no newRegions=true when running the playbook after the reboot
cd
rm -rf pmdk
```

## Installing and setting up a GitHub Actions runner
Installation and configuration of a self-hosted runner (as pmdkuser):
```sh
mkdir actions-runner && cd actions-runner
curl -o actions-runner-linux-x64-2.304.0.tar.gz -L https://github.com/actions/runner/releases/download/v2.304.0/actions-runner-linux-x64-2.304.0.tar.gz
tar xzf ./actions-runner-linux-x64-2.304.0.tar.gz
./config.cmd --url https://github.com/pmem/pmdk --token T_O_K_E_N__F_R_O_M__G_I_T_H_U_B
tmux
./run.cmd
```
Close session leaving the tmux session in the background (`CTRL+b d`).
