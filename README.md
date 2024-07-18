# XGDTool
XGDTool is an OG and 360 Xbox disc utility, capable of converting discs to and from any mainstream format. This is done without writing any temporary files, with the exception of one format.

## Features
### Seamless conversion
Seamless conversion is supported in the following formats:
- ISO / XISO
- Extracted files (Xex / Xbe / HDD Ready)
- GoD / Games on Demand
- CCI
- CSO

ZAR compressed archives can be directly created from any other format, but not the other way around (yet).

### Batch processing
XGDTool will automatically detect the type of file provided as an input argument, whether it's a game directory, GoD directory, game file, or directory full of games. If a directory full of games is provided, XGDTool will operate in "batch mode" and convert everything in the folder to whichever output format you've selected.

### Proper title lookup and patching
By default XGDTool will look up OG Xbox titles in an internal database, these titles will be used to patch your default.xbe files so they have proper names when viewing with a dashboard like XBMC. These names are also be used when creating game folders. 

For Xbox 360 titles, XboxUnity.net is queried for proper titles, which are used for folders, as well as Games on Demand Live header naming. This online functionality can be disabled, see the Usage section for how to do this.

### Attach XBE generation

## Usage
```XGDTool.exe <input_path> <output_directory> -output_format -options```

Providing anything except an input path is optional, the program will default to extraction when provided anything besides an extracted game, in that case it will create an XISO file.

### Output format arguments
These arguments are meant mutually exclusive, you can only use on at a time.
- ```-extract``` Extracts all files to a directory
- ```-xiso``` Creates an Xiso image
- ```-god``` Creates a Games on Demand image/directory structure
- ```-cci``` Creates a CCI archive (automatically split if too large for Xbox)
- ```-cso``` Creates a CSO archive (automatically split if too large for Xbox)
- ```-zar``` Creates a ZAR archive
- ```-xbe``` Generates an attach XBE file, does not convert the input file

## Options arguments
These arguments can be stacked, provide as many as you want, though not all output formats will use them. In that case the option is ignored. 
- ```-partial-scrub``` Scrubs and trims the output file, random padding data is removed. Compatible with XISO, CCI, CSO, GoD.
- ```-full-scrub``` Will completely reauthor the resulting file, this will produce the smallest file possible. Compatible with XISO, CCI, CSO, GoD.
- ```-split``` Splits the resulting XISO file if it's too large for OG Xbox.
- ```-attach``` Generates an attach XBE file if converting to XISO. Compatible with OG Xbox images.
- ```-am-patch``` Patches the "Allowed Media" field in resulting XBE files. Compatible with OG Xbox images.