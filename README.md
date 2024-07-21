# XGDTool
XGDTool is a command line OG and 360 Xbox disc utility, capable of converting discs to or from any mainstream format.

## Features
- Supports convertion between following formats:
    - ISO / XISO
    - Extracted files (Xex / Xbe / HDD Ready)
    - GoD / Games on Demand
    - CCI
    - CSO
    - ZAR
- Seamless conversion for almost all formats. For example, you can directly convert a GoD image to a ZAR archive, or an extracted game to a CCI archive, all without writing any temporary files. ZAR as an input format is the only one that doesn't support this seamless conversion yet.
- Batch processing, a folder full of different game formats can be batch converted to a single format with one command line argument.
- Option to select your target app/machine (Xemu, Xenia, OG Xbox, Xbox 360) and let XGDTool decide which settings to use.
- Attach XBE generation.
- Online database lookup for accurate file naming (can be disabled).

## Usage
```XGDTool.exe --output_format --settings <input_path> <output_directory>```

Settings and output directory are optionally.

### Output format arguments
These arguments are mutually exclusive, you can only use on at a time.
- ```--extract```    Extracts all files to a directory
- ```--xiso```       Creates an Xiso image
- ```--god```        Creates a Games on Demand image/directory structure
- ```--cci```        Creates a CCI archive (automatically split if too large for Xbox)
- ```--cso```        Creates a CSO archive (automatically split if too large for Xbox)
- ```--zar```        Creates a ZAR archive
- ```--xbe```        Generates an attach XBE file, does not convert the input file

"Choose for me" arguments:
- ```--ogxbox```    Choose the best format and settings for use with OG Xbox
- ```--xbox360```   Choose the best format and settings for use with Xbox 360
- ```--xemu```   Choose the best format and settings for use with Xemu
- ```--xenia```  Choose the best format and settings for use with Xenia

Information:
- ```--version``` Print version information
- ```--help``` Print usage information

## Settings arguments
These arguments can be stacked, though not all output formats will use them. In that case the option is ignored. If any are incompatible with one another, the first argument provided will be used. 
- ```--partial-scrub```  Scrubs and trims the output image, random padding data is removed.
- ```--full-scrub```     Completely reauthor the resulting image, this will produce the smallest file possible.
- ```--split```          Splits the resulting XISO file if it's too large for OG Xbox.
- ```--rename```         Patches the title field of resulting XBE files to one found in the database.
- ```--attach-xbe```         Generates an attach XBE file along with the output file.
- ```--am-patch```       Patches the "Allowed Media" field in resulting XBE files.
- ```--offline```        Disables online functionality.
- ```--debug```          Enable debug logging.
- ```--quiet```          Disable all logging except for warnings and errors.

# Build
If you have Ninja, Make, or MSVC installed and accessable in your environment's path, things should be fairly simple. XGDTool is setup with CMake so that it will automatically download and build all library dependancies with vcpkg, inside the project directory, just by configuring or building the project. 

The vcpkg toolchain file is linked in CMakeLists.txt already so you don't need to mess with it unless you'd like to use your own. In that case, comment out the lines pertaining to vckg at the top of CMakeLists.txt and add your own package manager's toolchain file. XGDTool relies on several libraries: lz4, zstd, nlohmann_json, cli11, openssl, curl

Some other dependancies (attach XBE and OG Xbox title database) are downloaded from github automatically as well, then converted to C headers. All this is done just by configuring or building the project with CMake. Review the CmakeLists.txt and .cmake files in the cmake directory to see what's going on.