subdir('tools')

# Setup run-tests target
run_tests_python = find_program('RUNTESTS.py')
run_tests_bash = find_program('RUNTESTS')
run_tests_powershell = find_program('RUNTESTS.PS1')
run_tests_python_args = []
run_tests_bash_args = []
run_tests_powershell_args = []
run_tests_env = {}
run_tests_python_env = {}
run_tests_bash_env = {}
run_tests_powershell_env = {}

# Translate meson_options to python/bash/powershell arguments

# Option: tests-case
tests_case = get_option('tests-case')
if tests_case != ''
    run_tests_python_args += tests_case
    run_tests_bash_args += tests_case
    run_tests_powershell_args += tests_case
endif

# Option: tests-fs-dir-force-pmem
tests_fs_dir_force_pmem = get_option('tests-fs-dir-force-pmem')
if tests_fs_dir_force_pmem
    # I think we can do it here instead of RUNTESTS.*
    run_tests_env += {'PMEM_IS_PMEM_FORCE':'1'}
endif

# Option: tests-suite-log-level
tests_suite_log_level = get_option('tests-suite-log-level')
run_tests_python_args += f'--unittest_log_level=@tests_suite_log_level@'
if tests_suite_log_level == 2
    run_tests_bash_args += '-v'
    run_tests_powershell_args += '-Verbose'
endif

# Option: tests-keep-going'
tests_keep_going = get_option('tests-keep-going')
if tests_keep_going
    run_tests_python_args += '--keep_going'
    run_tests_bash_env += {'KEEP_GOING':'1'}
    # this option is not supported in powershell I think
endif

# Option: tests-granularity
tests_granularity = get_option('tests-granularity')
if tests_granularity != 'any'
    run_tests_python_args += f'-g=@tests_granularity@'
    if tests_granularity == 'page'
        run_tests_bash_args += '-f=nonpmem'
        run_tests_powershell_args += '-f=non-pmem'
    elif tests_granularity == 'cacheline' or tests_granularity == 'byte'
        run_tests_bash_args += '-f=pmem'
        run_tests_powershell_args += '-f=pmem'
    elif tests_granularity == 'non'
        run_tests_bash_args += '-f=none'
endif

# Option: tests-type
tests_type = get_option('tests-type')
if tests_type != 'check'
    run_tests_python_args += f'-t=@tests_type@'
    run_tests_bash_args += f'-t=@tests_type@'
    run_tests_powershell_args += f'-t=@tests_type@'
endif

# Option: tests-timeout
tests_timeout = get_option('tests-timeout')
if tests_timeout != ''
    run_tests_python_args += f'-o@tests_timeout@'
    run_tests_bash_args += f'-o@tests_timeout@'
    run_tests_powershell_args += f'-o@tests_timeout@'
endif

# Option: tests-sequence
tests_sequence = get_option('tests-sequence')
if tests_sequence != ''
    run_tests_python_args += f'-u=@tests_sequence@'
    run_tests_bash_args += f'-u=@tests_sequence@'
    run_tests_powershell_args += f'-u=@tests_sequence@'
endif

# Create targets specific for each framework
run_tests_python_target = run_target(
    'run-tests-python',
    command:[run_tests_python]+run_tests_python_args,
    env:run_tests_env+run_tests_python_env,
)

run_tests_bash_target = run_target(
    'run-tests-bash',
    command:[run_tests_bash]+run_tests_bash_args,
    env:run_tests_env+run_tests_bash_env,
)

run_tests_powershell_target = run_target(
    'run-tests-powershell',
    command:[run_tests_powershell]+run_tests_powershell_args,
    env:run_tests_env+run_tests_powershell_env,
)

# If test case was specified we run only one framework
run_tests_targets = []
if tests_case != ''
    if fs.exists(meson.project_build_root() / 'src/test' / tests_case / 'TESTS.py')
        # Selected case is using python
        run_tests_targets += run_tests_python_target
    elif build_machine.system() == 'linux' and fs.exists(meson.project_build_root() / 'src/test' / tests_case / 'TEST0')
        # Selected case is using bash
        run_tests_targets += run_tests_bash_target
    elif build_machine.system() == 'windows' and fs.exists(meson.project_build_root() / 'src/test' / tests_case / 'TEST0.PS1')
        # Selected case is using powershell
        run_tests_targets += run_tests_powershell_target
    else
        error('No such a test case!')
    endif
else
    # Else, we run all available on the host machine
    run_tests_targets += run_tests_python_target
    if build_machine.system() == 'windows'
        run_tests_targets += run_tests_powershell_target
    else
        # We expect systems different than windows to be able to run bash
        run_tests_targets += run_tests_bash_target
    endif
endif

tests_target = alias_target(
    'run-tests',
    run_tests_targets,
)