Non-Volatile Memory Library

### NVML for Windows Installation ###

To install NVM Library on Windows:
* make sure that your system is 64-bit
* download installer
* run installer and follow the installation steps
* after installation sign out or reboot your system

Now you have access to system environment variables:

(Release installation)
* NVML_ExecutablePath - location of dll binaries and pmempool
* NVML_IncludePath - location of libraries header files
* NVML_LibraryPath - location of static lib files and debug symbols

(Debug installation)
* NVML_ExecutablePath_Dbg - location of dll binaries and pmempool
* NVML_IncludePath - location of libraries header files
* NVML_LibraryPath_Dbg - location of static lib files and debug symbols

You can use those variables in your Visual Studio projects
in place VC++ Directories or other properties like that:
`$(NVML_IncludePath)`

To add NVML variables to PATH just do:
set PATH=%PATH%;%NVML_ExecutablePath%
or for permanent changes do:
setx PATH=%PATH%;%NVML_ExecutablePath%

To uninstall library run installer again and select "Remove nvml" option.
It will remove all installed files, binaries and environemnt variables.

### NVML Windows Build Installer ###

To build NVM Library installer on Windows you need:

* MS Visual Studio 2015
* Microsoft Visual Studio 2015 Installer Projects
* Windows SDK 10.0.14393 (or later)

Open NVML.sln and build solution in Debug and Release configuration to
produce installer input, then build Setup/nvml_setup project.
As the result of nvml_setup build you will get msi file ready to publish or run.
