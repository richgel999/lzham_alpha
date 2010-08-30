LZHAM Codec - Alpha3 - Released Aug. 30 2010
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
		
Note: Levels 0-3 work fine but haven't been tweaked yet to strike the best balance of speed vs. compression ratio. Level 4 (uber) is currently the preferred mode.

See lzhamtest_x86/x64.exe's usage text for more command line parameters.


