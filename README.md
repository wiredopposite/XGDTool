# XGDTool
XGDTool is an OG and 360 Xbox disc utility, capable of converting discs to and from any mainstream format. It's available as a GUI or CLI app.

This program is still in initial testing. If you experience an issue, please report it in the Issues tab and help make this program better! Currently it's Windows only, but Linux support is planned.

## Features
- Supports convertion between following formats:
    - ISO / XISO
    - Extracted files (Xex / Xbe / HDD Ready)
    - GoD / Games on Demand
    - CCI
    - CSO
    - ZAR
- Seamless conversion, e.g. you can directly extract a GoD image, convert an ISO to ZAR archive, or CSO archive to CCI, without writing any temporary files. The only format requiring temporary files is ZAR when provided as input.
- Image scrubbing ("Partial Scrub"), gets rid of random padding and trims the output file to the shortest length possible.
- Image reauthoring ("Full Scrub"), completely rewrites the structure of the disc for the smallest possible output file.
- Image authoring, takes your extracted files and creates a new image with them.
- Multithreaded compression for CCI and CSO formats.
- Batch processing, a folder full of different game formats can be batch converted to a single format.
- Automatically finds split files when only one part is provided as an input path, assuming they're named in this format: ```name.1.extension``` ```name.2.extension```.
- Option to select your target app/machine (Xemu, Xenia, OG Xbox, Xbox 360) and let XGDTool decide which settings to use.
- Attach XBE generation for OG Xbox.
- Online database lookup for accurate file naming (can be disabled).

## CLI Usage
```XGDTool.exe <output_format> <settings_flags> <input_path> <output_directory>```

Settings flags and output directory are optional.

### Output format arguments (mutually exclusive)
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
- ```--list```      List contents of input file
- ```--version```   Print version information
- ```--help```      Print usage information

### Settings flags
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

## Build
### Windows
If you have Ninja, Make, or MSVC installed and accessable in your environment's path, things should be fairly simple. XGDTool is setup with CMake so that it will automatically download and build all library dependancies with vcpkg, inside the project directory, just by configuring or building the project. 

The vcpkg toolchain file is linked in CMakeLists.txt already so you don't need to mess with it unless you'd like to use your own. In that case, comment out the lines pertaining to vckg at the top of CMakeLists.txt and add your own package manager's toolchain file. XGDTool relies on several libraries: lz4, zstd, nlohmann_json, cli11, openssl, curl

Some other dependancies (attach XBE and OG Xbox title database) are downloaded from github automatically as well, then converted to C headers. All this is done just by configuring or building the project with CMake. Review the CmakeLists.txt and .cmake files in the cmake directory to see what's going on.

### Linux
This has only been tested with Clang and is still being worked on, you'll need to install some dependancies:
```
# Update and install deps
sudo apt update
sudo apt-get install cmake pkg-config liblz4-dev libzstd-dev libssl-dev libcurl4-openssl-dev libwxgtk3.0-gtk3-dev

# Clone with submodules
git clone --recursive https://github.com/wiredopposite/XGDTool.git
cd XGDTool

# Create a build dir
mkdir build
cd build

# Configure
cmake ..
# or if you need to specify clang
# cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ..

# Build
make

# Run
./XGDTool

```