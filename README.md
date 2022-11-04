# unlz2k
Research project on the compression algorithm used by TT Games.

The project's `build.bat` script requires the Visual Studio C++ Compiler (MSVC) to work. It generates the file `unlz2k.exe` in the project directory.

## Overview

LZ2K is a custom compression algorithm used in video games, particularly those by Traveller's Tales Games. Game data is packed into ambiguous .DAT files, which have packed into them compressed and uncompressed files. Though newer compression methods are used in later games, the older games only use LZ2K for the files they compress.

Because this is a custom algorithm, the source code for compression and decompression is not available. What exists publicly, however, is the decompression algorithm, packed into the raw binary of the distributed game executable. However, it relies on many other peripherals packed into the executable. When trying to understand this algorithm, simply dumping the algorithm and patching it doesn't quite get to the core of the file format and what it does in the first place. This is the reason for this project.

## File Format

TBD
