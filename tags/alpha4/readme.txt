LZHAM Codec - Alpha4 - Released Sept. 5, 2010
Copyright (c) 2009-2010 Richard Geldreich, Jr. <richgel99@gmail.com>
MIT License - http://code.google.com/p/lzham/

lzhamtest_x86/x64 is a simple command line test program that uses the LZHAM codec DLL to compress/decompress single files.

Usage examples:

- Compress single file "source_filename" to "compressed_filename":
	lzhamtest_x64 c source_filename compressed_filename
	
- Decompress single file "compressed_filename" to "decompressed_filename":
    lzhamtest_x64 d compressed_filename decompressed_filename

- Compress single file "source_filename" to "compressed_filename", then verify the compressed file decompresses properly to the source file:
	lzhamtest_x64 -v c source_filename compressed_filename

- Recursively compress all files under specified directory and verify that each file decompresses properly:
	lzhamtest_x64 -v a c:\source_path
	
Options	
	
- Set dictionary size used during compressed to 1MB (2^20):
	lzhamtest_x64 -d20 c source_filename compressed_filename
	
Valid dictionary sizes are [15,26] for x86, and [15,29] for x64. (See LZHAM_MIN_DICT_SIZE_LOG2, etc. defines in include/lzham.h.)
The x86 version defaults to 64MB (26), and the x64 version defaults to 256MB (28). I wouldn't recommend setting the dictionary size to 
512MB unless your machine has more than 4GB of physical memory.

- Set compression level to fastest:
	lzhamtest_x64 -m0 c source_filename compressed_filename
	
- Set compression level to uber (the default):
	lzhamtest_x64 -m4 c source_filename compressed_filename
	
- For best compression, use the -x option with -m4, which enables more rigorous (but ~4X slower!) parsing:
	lzhamtest_x64 -x -m4 c source_filename compressed_filename

See lzhamtest_x86/x64.exe's usage text for more command line parameters.

Compiling LZHAM

Out of the box, LZHAM can be compiled with Visual Studio 2008 (preferred) or with Codeblocks 10.05 using TDM-GCC x64 (GCC 4.5.0). 
http://www.codeblocks.org/
http://tdm-gcc.tdragon.net/

Visual Studio 2008 solution is "lzham.sln". The codec seems to compile and run fine with Visual Studio 2010 in my limited testing.

The codec compiles for Xbox 360 as well: lzham_x360.sln. Note that I barely spent any time verifying the codec on this platform.

The Codeblocks workspace is "lzhamtest.workspace". The codec runs a bit slower when compiled with GCC, but the difference is less than 5%.

Porting LZHAM

The decompressor LIB relies on the Win32 fiber API to easily support streaming. This will probably be the biggest stumbling block to porting the decompressor 
to a non-Win32 platform.

The compressor LIB relies on the Win32 semaphore and threading API's. Long term, I plan on adding support for pthreads.

