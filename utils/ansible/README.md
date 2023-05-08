Persistent Memory Development Kit

This is utils/ansible/README.

The scripts in this directory allow you to set up an RockyLinux and OpenSuSe

environment on a real HW and build a PMDK project in it.

To use playbooks it is required to install Ansible first. It must be done on computer that will be
 used to execute script, not on target platform.

Commands:

```ansible-playbook -i <target_platform_ip_address>, opensuse-setup.yml --extra-vars
 "host=all ansible_user=root ansible_password=<password_for_root_on_target> testUser=pmdkuser"```

```ansible-playbook -i <target_platform_ip_address>, rockylinux-setup.yml --extra-vars
 "host=all ansible_user=root ansible_password=<password_for_root_on_target> testUser=pmdkuser"```

Those commands above will install the required packages to build PMDK tests.

```ansible-playbook -i <target_platform_ip_address>, configure-pmem.yml --extra-vars
 "host=all ansible_user=root ansible_password=<password_for_root_on_target> testUser=pmdkuser"```

This command will configure regions and namespaces on DIMMs.

NOTE:

- If platform does not have DIMM's regions configured earlier you can add additional var for
 configureProvisioning.yml playbook: newRegions=true eg.

```ansible-playbook -i <target_platform_ip_address>, configureProvisioning.yml --extra-vars
 "host=all ansible_user=root ansible_password=<password_for_root> newRegions=true testUser=pmdkuser"```

This will reboot the platform.

Configured regions are required to create namespaces on the DIMMs.

- If Linux kernel is outdated, opensuseAnsible and rockyAnsible playbooks will reboot target platform.

- It is possible to run playbooks inside target platform but if notes above occurs then you need to rerun this playbook.

To run playbooks inside the platform please comment line:

` - hosts: "{{ host }}"`

and uncomment

` # - hosts: localhost` <br /> `#   connection: local`

and run with example command

`sudo ansible-playbook opensuse-setup.yml --extra-vars "testUser=pmdkuser"`
