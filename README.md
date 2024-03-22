# sc4-dbpf-loading

A DLL Plugin for SimCity 4 that optimizes the DBPF loading.   

This is an experimental plugin that provide the following features:

* Increases the game's internal file buffer size from 512 bytes to 4096 bytes.
* Disables the game code that searches the entire file if the DBPF signature was not found.
* Replaces one DBPF signature check with a file extension check, the plugin tells the game that any file with a 
.SC4 extension (.SC4, .SC4Lot, etc.) is DBPF. This removes one file open/read call, SC4 will still check the
signature when it loads the plugin file.
* Changes the 'missing plugin' dialog to display the plugin pack ID in hexadecimal (untested).
* Adds a `-StartupDBPFLoadTrace:` command line argument with the following options:
    * `ShowLoadTime` - shows a message box with the resource loading time in milliseconds.
    * `WinAPI` - shows message boxes before and after the resource loading code runs, this allows the user to start and stop a Process Monitor trace when the message box is shown.

The plugin can be downloaded from the Releases tab: https://github.com/0xC0000054/sc4-dbpf-loading/releases

## System Requirements

* SimCity 4 version 641
* Windows 10 or later

The plugin may work on Windows 7 or later with the [Microsoft Visual C++ 2022 x86 Redistribute](https://aka.ms/vs/17/release/vc_redist.x86.exe) installed, but I do not have the ability to test that.

## Installation

1. Close SimCity 4.
2. Copy `SC4DBPFLoading.dll` into the Plugins folder in the SimCity 4 installation directory.
3. Start SimCity 4.

## Troubleshooting

The plugin should write a `SC4DBPFLoading.log` file in the same folder as the plugin.    
The log contains status information for the most recent run of the plugin.

# License

This project is licensed under the terms of the MIT License.    
See [LICENSE.txt](LICENSE.txt) for more information.

## 3rd party code

[gzcom-dll](https://github.com/nsgomez/gzcom-dll/tree/master) Located in the vendor folder, MIT License.    
[EABase](https://github.com/electronicarts/EABase) Located in the vendor folder, BSD 3-Clause License.    
[EASTL](https://github.com/electronicarts/EASTL) Located in the vendor folder, BSD 3-Clause License.    
[SC4Fix](https://github.com/nsgomez/sc4fix) - MIT License.     
[.NET runtime](https://github.com/dotnet/runtime) The `Stopwatch` class is based on `System.Diagnostics.Stopwatch`, MIT License.    
[Windows Implementation Library](https://github.com/microsoft/wil) - MIT License    
[Boost.Algorithm](https://www.boost.org/doc/libs/1_84_0/libs/algorithm/doc/html/index.html) - Boost Software License, Version 1.0.    


# Source Code

## Prerequisites

* Visual Studio 2022

## Building the plugin

* Open the solution in the `src` folder
* Update the post build events to copy the build output to you SimCity 4 application plugins folder.
* Build the solution

## Debugging the plugin

Visual Studio can be configured to launch SimCity 4 on the Debugging page of the project properties.
I configured the debugger to launch the game in full screen with the following command line:    
`-intro:off -CPUcount:1 -w -CustomResolution:enabled -r1920x1080x32`

You may need to adjust the resolution for your primary screen.
