# XGDTool
XGDTool is a command line OG and 360 Xbox disc utility, capable of converting discs to and from any mainstream format.

This program is still in initial testing. If you experience an issue, please report it in the Issues tab and help make this program better! Currently it's Windows only, but Linux and Mac support are planned.

## Features
- Supports convertion between following formats:
    - ISO / XISO
    - Extracted files (Xex / Xbe / HDD Ready)
    - GoD / Games on Demand
    - CCI
    - CSO
    - ZAR (As output format only)
- Seamless conversion, you can directly extract a GoD image, convert an ISO to ZAR archive, or CSO archive to CCI, all without writing any temporary files.
- Batch processing, a folder full of different game formats can be batch converted to a single format with one command line argument.
- Automatically finds split files when only one part is provided as an input path, assuming they're named in this format: ```name.1.extension``` ```name.2.extension```.
- Option to select your target app/machine (Xemu, Xenia, OG Xbox, Xbox 360) and let XGDTool decide which settings to use.
- Attach XBE generation for OG Xbox.
- Online database lookup for accurate file naming (can be disabled).

## Usage
```XGDTool.exe --output_format --settings <input_path> <output_directory>```

Settings and output directory are optionally.

### Output format arguments
These arguments are mutually exclusive.
- ```--extract```   Extracts all files to a directory
- ```--xiso```      Creates an Xiso image
- ```--god```       Creates a Games on Demand image/directory structure
- ```--cci```       Creates a CCI archive (automatically split if too large for Xbox)
- ```--cso```       Creates a CSO archive (automatically split if too large for Xbox)
- ```--zar```       Creates a ZAR archive
- ```--xbe```       Generates an attach XBE file, does not convert the input file
- ```--ogxbox```    Automatically choose format and settings for use with OG Xbox
- ```--xbox360```   Automatically choose format and settings for use with Xbox 360
- ```--xemu```      Automatically choose format and settings for use with Xemu
- ```--xenia```     Automatically choose format and settings for use with Xenia

Information:
- ```--list```      List file contents of input image
- ```--version```   Print version information
- ```--help```      Print usage information

### Settings arguments
These arguments can be stacked, though not all output formats will use them, in that case the option is ignored. If any conflicting settings are provided (e.g. full/partial scrub), the last one will be used. 
- ```--partial-scrub```  Scrubs and trims the output image, random padding data is removed.
- ```--full-scrub```     Completely reauthor the resulting image, this will produce the smallest file possible.
- ```--split```          Splits the resulting XISO file if it's too large for OG Xbox.
- ```--rename```         Patches the title field of resulting XBE files to one found in the database.
- ```--attach-xbe```     Generates an attach XBE file along with the output file.
- ```--am-patch```       Patches the "Allowed Media" field in resulting XBE files.
- ```--offline```        Disables online functionality.
- ```--debug```          Enable debug logging.
- ```--quiet```          Disable all logging except for warnings and errors.

### Example arguments
Produces a scrubbed CCI image and attach Xbe ready for use with OG Xbox:
```XGDLog.exe --cci --partial-scrub --attach-xbe "input_file.iso" "output/directory"```
Produces a GoD image ready for use with Xbox 360:
```XGDLog.exe --god "input/xex_directory" "output/directory"```

## Build
If you have Ninja, Make, or MSVC installed and accessable in your environment's path, things should be fairly simple. XGDTool is setup with CMake so that it will automatically download and build all library dependancies with vcpkg, inside the project directory, just by configuring or building the project. 

The vcpkg toolchain file is linked in CMakeLists.txt already so you don't need to mess with it unless you'd like to use your own. In that case, comment out the lines pertaining to vckg at the top of CMakeLists.txt and add your own package manager's toolchain file. XGDTool relies on several libraries: lz4, zstd, nlohmann_json, cli11, openssl, curl

Some other dependancies (attach XBE and OG Xbox title database) are downloaded from github automatically as well, then converted to C headers. All this is done just by configuring or building the project with CMake. Review the CmakeLists.txt and .cmake files in the cmake directory to see what's going on.