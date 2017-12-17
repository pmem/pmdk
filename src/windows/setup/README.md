Persistent Memory Development Kit

### PMDK for Windows Installation ###

To install Persistent Memory Development Kit on Windows:
* make sure that your system is 64-bit with installed
Visual C++ Redistributable for Visual Studio 2015 (or later)
* download installer from [github release page](https://github.com/pmem/pmdk/releases)
* run installer and follow the installation steps
* after installation sign out or reboot your system

Now you have access to system environment variables:

(Release installation)
* PMDK\_ExecutablePath - location of dll binaries and pmempool
* PMDK\_IncludePath - location of libraries header files
* PMDK\_LibraryPath - location of static lib files and debug symbols

(Debug installation)
* PMDK\_ExecutablePath\_Dbg - location of dll binaries and pmempool
* PMDK\_IncludePath - location of libraries header files
* PMDK\_LibraryPath\_Dbg - location of static lib files and debug symbols

You can use those variables in your Visual Studio projects
in place VC++ Directories or other properties like that:
`$(PMDK_IncludePath)`

To add PMDK variables to PATH just do:

```
set PATH=%PATH%;%PMDK_ExecutablePath%
```

or for permanent changes like administrator do:

```
setx /M PATH "%PATH%;%PMDK_ExecutablePath%"
```

To uninstall library run installer again and select "Remove pmdk" option.
It will remove all installed files, binaries and environemnt variables.

### PMDK Windows Build Installer ###

To build PMDK installer on Windows you need:

* MS Visual Studio 2015
* [Microsoft Visual Studio 2015 Installer Projects](https://marketplace.visualstudio.com/items?itemName=VisualStudioProductTeam.MicrosoftVisualStudio2015InstallerProjects)
* [Windows SDK 10.0.14393 (or later)](https://developer.microsoft.com/en-US/windows/downloads)

Open PMDK.sln and build solution in Debug and Release configuration to
produce installer input, then build Setup/pmdk\_setup project.
As the result of pmdk\_setup build you will get msi file ready to publish or run.
