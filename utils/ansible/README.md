# Persistent Memory Development Kit

This is utils/ansible/README.md.

The scripts in this directory allow you to set up an RockyLinux and OpenSuSe

environment on a real HW and build a PMDK project in it.

To use playbooks it is required to install Ansible first. It must be done
on computer that will be used to execute script, not on target platform.

```
dnf install ansible-core
```
or
```
zypper install ansible
```

Use the commands below to setup PMDK software development environment:
```
ansible-playbook -i <target_platform_ip_address>, opensuse-setup.yml --extra-vars
 "host=all ansible_user=root ansible_password=<password_for_root_on_target> testUser=pmdkuser testUserPass=pmdkpass"
 ```
or
```
ansible-playbook -i <target_platform_ip_address>, rockylinux-setup.yml --extra-vars
 "host=all ansible_user=root ansible_password=<password_for_root_on_target> testUser=pmdkuser testUserPass=pmdkpass"
 ```

Use the below commands to configure persistent memory on Intel servers
(regions and namespaces on DIMMs) to be used for PMDK libraries tests execution.
```
ansible-playbook -i <target_platform_ip_address>, configure-pmem.yml --extra-vars
 "host=all ansible_user=root ansible_password=<password_for_root_on_target> testUser=pmdkuser"
 ```

NOTE:

- If platform does not have DIMM's regions configured earlier you can add additional var for
 configureProvisioning.yml playbook: newRegions=true eg.

```
ansible-playbook -i <target_platform_ip_address>, configureProvisioning.yml --extra-vars
 "host=all ansible_user=root ansible_password=<password_for_root> newRegions=true testUser=pmdkuser"
 ```

This will reboot the platform.

Configured regions are required to create namespaces on the DIMMs.

- If Linux kernel is outdated, opensuseAnsible and rockyAnsible playbooks will reboot target platform.

- It is possible to run playbooks inside target platform but if notes above occurs then you need to rerun this playbook.

To run playbooks inside the platform please comment line:

` - hosts: "{{ host }}"`

and uncomment

```
# - hosts: localhost
#   connection: local
```

and run with example command

`sudo ansible-playbook opensuse-setup.yml --extra-vars "testUser=pmdkuser"`

# GitHub self-hosted runner setup
The sequence of commands below presents an example procedure how to setup
a new server to be used as self-hosted GHA runner.
## System setup and configuration (assuming base OS is already installed)
### As root (for RockyLinux):
```
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
cd
rm -rf pmdk
```
### As root (for OpenSuSe):
```
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
cd
rm -rf pmdk
```
## A self-hosted runner setup and configuration

As pmdkuser (self hosted runner installation and configuration):
```
mkdir actions-runner && cd actions-runner
curl -o actions-runner-linux-x64-2.304.0.tar.gz -L https://github.com/actions/runner/releases/download/v2.304.0/actions-runner-linux-x64-2.304.0.tar.gz
tar xzf ./actions-runner-linux-x64-2.304.0.tar.gz
./config.cmd --url https://github.com/pmem/pmdk --token T_O_K_E_N__F_R_O_M__G_I_T_H_U_B
tmux
./run.cmd
```
Close session leaving tmux session in background.
