Non-Volatile Memory Library

### NVML for Windows Installation ###

To install NVM Library on Windows:
* make sure that your system is 64-bit
* download installer
* run installer and follow the installation steps
* after installation sign out or reboot your system

Now you can use pmempool from each command line.

Additionally, you have access to system environment variables:
* NVML_IncludePath - location of libraries header files
* NVML_LibraryPath - location of static lib files and debug symbols

You can use those variables in your Visual Studio projects
in place VC++ Directories or other properties:
`$(NVML_IncludePath)`

To uninstall library run setup.exe and select "Remove nvml" option.

### NVML Windows Build Installer ###

To build NVM Library installer on Windows you need:

* MS Visual Studio 2015
* Microsoft Visual Studio 2015 Installer Projects
* Windows SDK 10.0.14393 (or later)

Open NVML.sln and build Setup/nvml_setup.
As the result of build you will get setup and msi file.

>NOTE:
If you want to change build configuration from Debug to Release or
otherwise at first run SET_LIB.PS1 script with params:
SET_LIB.PS1 -projectdir <path_to_setup_dir> -configuration <Debug|Release>
and then build nvml_setup as previously.
