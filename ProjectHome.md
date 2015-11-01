Note: LZHAM v1.0 has relocated to github [here](https://github.com/richgel999/lzham_codec). This old Google Code site only has the slower alpha versions. New versions on github are NOT backwards compatible with the alpha versions here.

**LZHAM** (LZ, Huffman, Arithmetic, Markov) **Alpha8** is a general purpose lossless data compression library that borrows several ideas from [LZMA](http://www.7-zip.org/sdk.html) but purposely makes several key tradeoffs that favor decompression speed over compression ratio. LZHAM's  compression ratio is a bit less than LZMA, but decompresses approximately 2-3x faster on a Core i7.
LZHAM's decompressor is intended to be particularly fast on embedded devices, handhelds and game console platforms.

This is an alpha release. The codec has been tested on several terabytes of data, but more work needs to be done on compression speed, portability, and tuning the decompressor's inner loop for various platforms.

LZHAM is now listed on Matt Mahoney's [Large Text Compression Benchmark](http://mattmahoney.net/dc/text.html) page. Also, this [wiki page](statistics.md) details how well the latest version of LZHAM compares to other codecs.

For a description of the compression techniques used by this library, see [this page](http://code.google.com/p/lzham/wiki/CompressionTechniquesUsed). For documentation, see [this page](http://code.google.com/p/lzham/wiki/API_Docs).

Note if you're looking for a smaller, faster compression library written in C, check out my [miniz.c](http://code.google.com/p/miniz/) project. It's a public domain Inflate/Deflate codec with a zlib API, a simple PNG writer, and a ZIP archive reader/writer implementation in a single source code file.

**Version 1.0 Status**

[See my blog.](http://richg42.blogspot.com/2015/01/lzham-v10-progress.html) It's coming! I'll probably be releasing v1.0 to github. Faster, more stable decompression, lower initialization times, lower decompression memory usage.

I've been testing LZHAM v1.0 on [iOS devices](http://richg42.blogspot.com/2015/01/first-lzham-ios-stats-with-unity-asset.html).

**What Uses LZHAM**

Two big games I know about that use LZHAM are [PlanetSide 2](http://en.wikipedia.org/wiki/PlanetSide_2) and [Titanfall](http://en.wikipedia.org/wiki/Titanfall).

**Features**

  * Very fast single threaded decompression speed (typically 75-110MB/sec. on a 2.6GHz Core i7) combined with high compression ratios and low decompression memory requirements. To the author's knowledge, very few (if any) stronger open-source codecs decompress faster.
  * Contains a zlib API compatibility layer (in Alpha8). LZHAM can be a drop-in replacement for zlib.h in many cases. If you already know or use the zlib API, you'll feel right at home using LZHAM's zlib-style API.
  * Usable as a DLL or LIB using a simple interface (see include/lzham.h) for x86 Win32/Linux and x64 Win32 platforms. Codec has also been minimally tested on Xbox 360.
  * Supports a true streaming API (the decompressor function is implemented as a coroutine), or block based (unbuffered/memory to memory) compression/decompression API's.
  * Compressor is heavily multithreaded in a way that does not sacrifice compression ratio.
  * Supports fully deterministic compression (independent of platform, compiler, or optimization settings), or non-deterministic compression at higher performance.
  * Supports any power of 2 dictionary size from 32KB up to 64MB on x86 platforms, and up to 512MB on x64.
  * Minimal expansion of uncompressable data using raw blocks. Maximum expansion is roughly 4 bytes per 512KB at typical dictionary sizes. Decompressor handles raw blocks very efficiently (using memcpy()).
  * Supports static (seed) dictionaries, which are useful for creating compressed patch files ([differential/delta compression](http://en.wikipedia.org/wiki/Delta_encoding)), or increased compression when compressing files from known sources.

**Linux Support**

[2/2/14] - The current code in SVN has cmake files and should compile out of the box using gcc v4.8 or clang v3.4. Example on how to build for x64 Release:

```
  cmake . -DBUILD_X64=on -DCMAKE_BUILD_TYPE=Release
  make clean
  make
```

Currently, I've only written cmake files lzhamtest and the compression/decompression libraries, and it only supports static linking.

**Upcoming Version (Alpha8)**

I've checked in the next version (Alpha8) to SVN. I'll be exhaustively testing the codec over the next few days before I create an archive. The latest "stable" version of LZHAM is still alpha7. Note that alpha8 bitstreams are NOT compatible with alpha7, due to a small change I had to make to the block header to support zlib-style flushing. Here's a description of the major changes/fixes:

Alpha8 leverages my experience creating the "miniz" project. I've added a significant subset of the zlib API to LZHAM: compression, decompression, etc. with all flush modes. This is done by layering the zlib functions on top of the existing lower level API. lzham.h can act as a drop in replacement for zlib.h in most cases, so no changes are required to the calling code. I've tested against libpng and libzip so far.

Alpha8's [lzham.h](http://code.google.com/p/lzham/source/browse/trunk/include/lzham.h) include file is now includable from .C sources files. Also, I've added four new examples (two are written in plain C). Alpha8 now supports static and dynamic linkage via two different libs (lzhamlib vs. lzhamdll).

I added a new optional namespace named "lzham\_ex" which includes my stream I/O classes with built in support for streaming compression/decompression. See example4.

The return values from `lzham_compress()` and `lzham_decompress()` are interpreted slightly differently vs. previous releases. The codec returns a value >= `LZHAM_COMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE` on completion or failure. Please search for `LZHAM_COMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE` in [lzhamtest.cpp](http://code.google.com/p/lzham/source/browse/trunk/lzhamtest/lzhamtest.cpp) for an example on how to properly interpret the return codes.

Optimized `lzham_compress_init()` (2-3x faster). Now takes around 1.3-1.5ms on a Core i7 3.2GHz. Also optimized `lzham_decompress_init()` and `lzham_decompress_reinit()`.

I added `lzham_compress_reinit()`, which is ~15x faster than calling `lzham_compress_init()`. `lzham_compress_reinit()` reuses all of the current compression state (helper threads, allocated memory, etc.), making it much faster.

I added support for various zlib-style flush modes to the compressor. This allows the caller to efficiently flush the compressor in various ways (full flush, sync flush, symbol table update rate flush), as well as sync the output bitstream to a byte boundary. This is useful for packet and record compression. Explicit table update rate flushing is useful when the caller knows the upcoming data statistics are going to dramatically change (like when appending multiple files to solid archives).

The decompressor now supports decoding packets/blocks of compressed data blobs created using the newly added flush functionality. This works as long as the compressor issues a full flush after each packet. Packet decompression can occur in any order (as long as the first packet, which contains the header, is decoded first). The decompressor always does a coroutine return when it sees a full flush.

Created more examples, and updated the API doc wiki.

Still to do: More/better docs, Mac/BSD support, add the prefix table update rate to the compressed bitstream's header (it's currently hardcoded).

**Version History**
  * alpha8 - Feb. 2, 2014 - On SVN only: Project now has proper Linux cmake files. Tested and fixed misc. compiler warnings with clang v3.4 and gcc v4.8, x86 and amd64, under Ubuntu 13.10. Added code to detect the # of processor cores on Linux, fixed a crash bug in lzhamtest when the source file was unreadable, lzhamtest now defaults to detecting the # of max helper threads to use.
  * alpha8 - Mar. 6, 2012 - Currently released to SVN only. Still undergoing testing.
  * alpha7 - Dec. 15, 2011 - Delta compression, API bugfixes found during 7-zip integration, initial large match support, few small ratio increases, more docs in lzham.h
  * alpha6 - Jan. 9, 2011 - Determinism fixes, removed unnecessary memcpy() from compressor, moved the per-file MIT license text to a single place at the end of include/lzham.h (like LUA does).
  * alpha5 - Oct. 17, 2010 - 32-bit Linux and ANSI C/C++ compatibility, decompressor implemented as coroutine vs. a Fiber, improved decompression speed, minor bitstream format cleanup.
  * alpha4 - Sept. 5, 2010 - Faster compression, especially on lower end CPU's, using a more efficient (and simpler) parser, match finder optimizations.
  * alpha3 - Faster compression, first ports to various platforms/compilers, command line options added to lzhamtest\_x86/x64.
  * alpha2 - Fixed a rare bug in the binary arithmetic decoder's renormalization logic. The original logic (derived from FastAC with changes to simplify interleaving arithmetic and Polar/Huffman codes into a single linear bitstream) was fine, but somehow in a late night optimization session I messed this up without catching it.
  * alpha1 - Initial release.

**Known Issues**
  * lzham\_decompress\_reinit() does not reset the decompressor's arithmetic tables. This was fixed in LZHAM v1.0. (It's trivial easy to back port the fix into this version if you need to - email me.)
  * LZHAM is intentionally a **highly** asymmetric codec. Typical decompression throughput is 30-40x faster than compression throughput on multicore machines. This codec is primarily intended for data distribution: compress once on a high end box, decompress many times (at a high throughput) out in the field. It's not intended to be used for real-time stream compression. (Alpha8 will improve this situation somewhat with its support for flushing and `lzham_compress_reinit()`).
  * Compression performance on very small files isn't as high as it could be (and was probably higher using alpha2's parser). The next version of the compressor will update the symbol statistics much more frequently at the very beginning of files.
  * Load balancing issues - the compressor rarely utilizes 100% of a multicore CPU (usually more like 70-80%). Overall, match finding is the top bottleneck with large dictionaries.
  * The compression engine still uses more total CPU relative to LZMA. On older non-multicore CPU's the compressor is still a bit slow in "uber" mode.
  * "Extreme" parsing can be very, very slow on some files. On some files the extreme parser (-x in lzhamtest) can become pathologically slow (but it will always make forward progress).
  * On very uncompressible files the decompressor can spend way too much time updating Huffman tables, negating its perf. advantage vs. LZMA.
  * The compressor's initialization time is relatively high (several ms) - it is just not designed to be used on small blocks. I've spent relatively more time trying to reduce the decompressor's init time.
  * The compressor forks and joins a lot, which limits the potential speedup. The next major release will attempt to address this issue.

**Quick Test Executable Instructions**
  * The precompiled EXE's under Downloads where compiled with Visual Studio 2008. (I also included a 32-bit Linux executable in the source archive: bin\_linux/lzhamtest\_x86.) You may need the VC++ 2008 runtime installed on your system to run these executables:
> http://www.microsoft.com/downloads/details.aspx?familyid=A5C84275-3B97-4AB7-A40D-3802B2AF5FC2&displaylang=en

> Note: As of Alpha2 the VC runtime shouldn't be needed.

  * "bin/lzhamtest\_x86" (or bin/lzhamtest\_x64) is a simple command line test app. Use the "c" and "d" options to compress individual files:
```
  lzhamtest_x64 c input_file compressed_file
  lzhamtest_x64 d compressed_file decompressed_file
```
  * The "a" option can be used to recursively compress/decompress/compare an entire directory of test files. (The -v option enables verification by decompressing the compressed file and comparing the output to the original file.) Note that lzhamtest\_x86/x64 is not intended to be used as an archiver, so it doesn't preserve the temporary output files it generates during compression in this mode.
```
  lzhamtest_x64 -v a C:\testfiles
```
  * Running lzhamtest\_x86/x64 without any options tests the codec with a simple string to verify basic functionality.
  * The compressed data stream always contains the Adler-32 of the input data at the very end. By default, the decompressor verifies the decompressed data's validity against this checksum.
  * Out of the box, lzhamtest\_x86 uses a 64MB dictionary and the LZHAM\_COMP\_LEVEL\_UBER compression level. lzhamtest\_x64 uses 256MB.

**Support Contact**

For any questions or problems with this codec please contact [Rich Geldreich](http://www.mobygames.com/developer/sheet/view/developerId,190072/) at <richgel99 at gmail.com>. Here's my [twitter page](http://twitter.com/#!/richgel999).