Non-Volatile Memory Library

### NVML for Windows Installation ###

To install NVM Library on Windows:
* make sure that your system is 64-bit with installed
Visual C++ Redistributable for Visual Studio 2015 (or later)
* download installer from [github release page](https://github.com/pmem/nvml/releases)
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

```
set PATH=%PATH%;%NVML_ExecutablePath%
```

or for permanent changes like administrator do:

```
setx /M PATH "%PATH%;%NVML_ExecutablePath%"
```

To uninstall library run installer again and select "Remove nvml" option.
It will remove all installed files, binaries and environemnt variables.

### NVML Windows Build Installer ###

To build NVM Library installer on Windows you need:

* MS Visual Studio 2015
* [Microsoft Visual Studio 2015 Installer Projects](https://marketplace.visualstudio.com/items?itemName=VisualStudioProductTeam.MicrosoftVisualStudio2015InstallerProjects)
* [Windows SDK 10.0.14393 (or later)](https://developer.microsoft.com/en-US/windows/downloads)

Open NVML.sln and build solution in Debug and Release configuration to
produce installer input, then build Setup/nvml_setup project.
As the result of nvml_setup build you will get msi file ready to publish or run.
