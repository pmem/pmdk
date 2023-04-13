#!/usr/bin/python

#
# Copyright 2019-2023, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

import os
from ansible.module_utils.basic import AnsibleModule

DOCUMENTATION = '''
---
module: env

short_description: Module for managing environmental variables on remote hosts

description:
    - "Module for set and unset environmental variables on remote hosts.
       Setting is done by writing export definitions to the file* (this should help with keeping persistence of values
       between reboots) and then execute 'source' command (in order to feed current session with exported variables).
       
    - Unsetting is done by removing export definitions from the file* and call 'unset' command.
       
    - Keep in mind that this module is designed to manage custom variables, not system's (e. g. when unset var which
       is not in the file*, after reboot it will be set again.
    - ----
    -  * file: the file under the care of this module, specified by 'path' parameter. By default it is a file in 
       /etc/profile.d/"

options:
    name:
        description:
            - This is the name of the variable to manage
        required: true
    value:
        description:
            - This is the value of the variable to manage. If 'state'='present', this option is required. If 'state'='absent', this option is forbidden.
        required: false
    state:
        description:
            - If 'present', ensure that given variable is set to given value. This is the default.
            - If 'absent', ensure that given variable is not set.
        required: false
        default: present
        choices: [ absent, present ]
    path:
        description:
            - This is the path of a file where export definitions will be written. Usually you don't want to change it.
        required: false
        default: /etc/profile.d/ansible_env_module_definitions.sh
            
author:
    - dpysx
'''

EXAMPLES = '''
# Set http_proxy:
- name: set http proxy
  env:
    state: present
    name: http_proxy
    value: http://example.com:80

# Make sure that EXAMPLE_DEBUG variable is not set:
- name: Unset EXAMPLE_DEBUG
  env:
    state: absent
    name: EXAMPLE_DEBUG
'''

RETURN = '''
old_value:
    description: Value of variable, before the module was called.
    type: str
'''


# #######################################################################################
# ##########################  General helpers  ##########################################
# #######################################################################################


def check_prerequisites(module, result):
    """Check if passed module and result are valid types from AnsibleModule."""
    if not isinstance(module, AnsibleModule):
        raise TypeError("Given module is not valid!")

    if not isinstance(result, dict):
        raise TypeError("Given result is not valid!")


# #######################################################################################
# ###################  Helpers for dealing with system commands  ########################
# #######################################################################################


def escape_quotes(string):
    """Escape every double quote char with a backslash."""
    return string.replace('"', '\\"')


def run_command(module, result, command):
    """Execute command and return its stdout; when status not zero, fail module execution."""
    check_prerequisites(module, result)

    command = 'bash -lc "' + escape_quotes(command) + '"'

    rc, stdout, stderr = module.run_command(command)

    if rc != 0:
        fail_message = "Command '" + command + "' failed with status code " + str(
            rc) + ", stdout: '" + stdout + "', stderr: '" + stderr + "'."
        module.fail_json(msg=fail_message, **result)

    return stdout


# #######################################################################################
# ######################  Helpers for handling checking env vars  #######################
# #######################################################################################


def get_env_value_or_empty(module, result, name):
    """Obtain value of variable and return it; if not present, return empty string."""
    check_prerequisites(module, result)

    print_variable = 'echo "$' + name + '"'
    variable_value = run_command(module, result, print_variable)

    return variable_value.replace('\n', '')


def is_env_present(module, result, name):
    """Check if variable with given name is present in system."""
    check_prerequisites(module, result)

    env_value = get_env_value_or_empty(module, result, name)
    if env_value == "":
        return False
    else:
        return True


# #######################################################################################
# ##########################  Helpers for file handling  ################################
# #######################################################################################


valid_shebang = "#!/usr/bin/env bash\n"


def read_file_lines(module, result, filename):
    """Read file and return its lines in a form of list."""
    content = []
    try:
        file = open(filename, "r")
        content = file.readlines()
        file.close()
    except IOError:
        fail_message = "Cannot open file '" + filename + "' for reading."
        module.fail_json(msg=fail_message, **result)

    return content


def check_existence(filename):
    """Check if file exists."""
    return os.access(filename, os.F_OK)


def check_r_permission(filename):
    """Check if file has read permissions."""
    return os.access(filename, os.F_OK) and os.access(filename, os.R_OK)


def check_w_permission(filename):
    """Check if file has write permissions."""
    return os.access(filename, os.F_OK) and os.access(filename, os.W_OK)


def check_x_permission(filename):
    """Check if file has executable permissions."""
    return os.access(filename, os.F_OK) and os.access(filename, os.X_OK)


def is_valid_shebang(module, result, filename):
    """Check if file has valid, bash shebang line."""
    check_prerequisites(module, result)

    file_lines = read_file_lines(module, result, filename)

    global valid_shebang
    if len(file_lines) == 0 or file_lines[0] != valid_shebang:
        return False
    else:
        return True


def write_file_lines(module, result, filename, lines):
    """Write all lines to the file."""
    check_prerequisites(module, result)

    changed = False
    if not check_existence(filename):
        changed = True

    try:
        file = open(filename, 'w')
    except IOError:
        fail_message = "Cannot open file '" + filename + "' for write."
        module.fail_json(msg=fail_message, **result)

    try:
        content = []
        for line in lines:
            if not line.endswith('\n'):
                s = line + "\n"
            else:
                s = line
            content.append(s)

        file.writelines(content)
        changed = True
    except:
        fail_message = "Cannot write to file '" + filename + "'."
        module.fail_json(msg=fail_message, **result)
    finally:
        file.close()

    result['changed'] = changed


def write_variable_to_file(module, result, name, value, filename):
    """Write export definition of the variable to the file. Removes possible duplicates."""
    check_prerequisites(module, result)
    global valid_shebang

    content = []
    env_entry_start = "export " + name + "="
    env_entry = env_entry_start + '"' + escape_quotes(value) + '"\n'

    if check_existence(filename):
        if not check_r_permission(filename):
            fail_message = "Cannot open file '" + filename + "' for read."
            module.fail_json(msg=fail_message, **result)

        if not check_w_permission(filename):
            fail_message = "Cannot open file '" + filename + "' for write."
            module.fail_json(msg=fail_message, **result)

        content = read_file_lines(module, result, filename)

        if not is_valid_shebang(module, result, filename):
            content.insert(0, valid_shebang)

        # remove all entries for current variable:
        filtered_content = []
        for line in content:
            if not line.startswith(env_entry_start):
                filtered_content.append(line)

        content = filtered_content
        content.append(env_entry)

        write_file_lines(module, result, filename, content)
        result['changed'] = True

    else:
        content.append(valid_shebang)
        content.append(env_entry)
        write_file_lines(module, result, filename, content)
        try:
            os.chmod(filename, 0o755)
            result['changed'] = True
        except:
            fail_message = "Cannot change file '" + filename + "' permissions to 755."
            module.fail_json(msg=fail_message, **result)


def remove_variable_from_file(module, result, name, filename):
    """Remove export definition of the variable form the file."""
    check_prerequisites(module, result)

    if not check_existence(filename):
        return

    if not check_r_permission(filename):
        fail_message = "Cannot open file '" + filename + "' for read."
        module.fail_json(msg=fail_message, **result)

    if not check_w_permission(filename):
        fail_message = "Cannot open file '" + filename + "' for write."
        module.fail_json(msg=fail_message, **result)

    content = read_file_lines(module, result, filename)

    content_change = False

    new_content = []
    for entry in content:
        if entry.startswith("export " + name + "="):
            content_change = True
        else:
            new_content.append(entry)

    if content_change:
        write_file_lines(module, result, filename, new_content)


# #######################################################################################
# ###############  Helpers for exporting / unset vars to current session  ###############
# #######################################################################################


def import_variables_from_file(module, result, filename):
    """Execute 'source' command with file parameter in order to import definitions to the current session."""
    check_prerequisites(module, result)

    if not check_existence(filename):
        return

    if not check_r_permission(filename):
        module.fail_json(msg="Cannot read file '" + filename + "'!", **result)

    result['changed'] = True

    load_variables_from_file = 'source "' + filename + '"'
    run_command(module, result, load_variables_from_file)


def unset_variable(module, result, name):
    """Execute 'unset' command in order to remove exported definition."""
    check_prerequisites(module, result)

    result['changed'] = True
    unset_var = "unset " + name
    run_command(module, result, unset_var)


def export_variable(module, result, name, value):
    """Execute 'export' command in order to add exported definition."""
    check_prerequisites(module, result)

    result['changed'] = True
    export_var = "export " + name + '="' + escape_quotes(value) + '"'
    run_command(module, result, export_var)


# #######################################################################################
# ##########################  Main module functions  ####################################
# #######################################################################################


def absent_flow(module, result, name, filename):
    """Execute module flow for 'absent' module mode."""
    check_prerequisites(module, result)

    if not is_env_present(module, result, name):
        module.exit_json(**result)

    current_value = get_env_value_or_empty(module, result, name)
    result['old_value'] = current_value
    result['changed'] = True

    if not module.check_mode:
        unset_variable(module, result, name)
        remove_variable_from_file(module, result, name, filename)

    module.exit_json(**result)


def present_flow(module, result, name, value, filename):
    """Execute module flow for 'present' module mode."""
    check_prerequisites(module, result)

    current_value = get_env_value_or_empty(module, result, name)
    result['old_value'] = current_value

    if value is not None:

        if is_env_present(module, result, name) and current_value == value:
            module.exit_json(**result)

        result['changed'] = True

        if not module.check_mode:
            write_variable_to_file(module, result, name, value, filename)
            import_variables_from_file(module, result, filename)
            export_variable(module, result, name, value)

        module.exit_json(**result)

    else:
        if is_env_present(module, result, name):
            module.exit_json(**result)
        else:
            module.fail_json("variable is not set!", **result)


def run_module():
    """Prepare parameter and return value object, select and execute proper module flow."""

    module_args = dict(
        state=dict(type='str', default='present', choices=['present', 'absent']),
        path=dict(type='str', default='/etc/profile.d/ansible_env_module_definitions.sh'),
        name=dict(type='str', required=True),
        value=dict(type='str'),
    )

    result = dict(
        changed=False,
        old_value=''
    )

    module = AnsibleModule(
        argument_spec=module_args,
        supports_check_mode=True
    )

    state = module.params['state']
    name = module.params['name']
    value = module.params['value']
    path = module.params['path']

    # arguments' reasonableness check
    if state == "present" and value is None:
        module.fail_json(msg="For 'state'='present' there should be 'value' set as well!", **result)

    if state == "absent" and value is not None:
        module.fail_json(msg="For 'state'='absent' there should be no 'value' set!", **result)

    # execute proper flow:
    if state == "absent":
        absent_flow(module, result, name, path)
    else:
        present_flow(module, result, name, value, path)

    # flow functions should end module's execution; when otherwise, there is some ancient evil in here:
    module.fail_json(msg="Unexpected execution path, this is a bug in this module!", **result)


def main():
    run_module()


if __name__ == '__main__':
    main()
