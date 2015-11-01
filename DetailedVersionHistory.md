**Alpha8 Changes - March 6, 2012**

Currently only released to SVN head. Not tested under Linux yet.

**Alpha7 Changes**

Latest stable version.

Alpha7's compressed bitstream is NOT compatible with previous versions. (Sorry, but that's why I'm using the "Alpha" designation. I'll be locking LZHAM's bitstream sometime next year.)

Finally wrote some [documentation](http://code.google.com/p/lzham/wiki/API_Docs).

Alpha7 supports static (seed) dictionaries, which are useful for creating compressed patch files ([differential/delta compression](http://en.wikipedia.org/wiki/Delta_encoding)). Seed dictionary sizes are limited to the size of the compresssor's dictionary size (i.e. max of 512MB for x64, and 64MB for x86). See the -a option in lzhamtest, or the m\_num\_seed\_bytes/m\_pSeed\_bytes members of the compression/decompression param structs in include/lzham.h.

I added much more documentation to the main header file include/lzham.h.

I added the LZHAM\_MORE\_FREQUENT\_TABLE\_UPDATING macro to lzham\_symbol\_code.cpp. It's jammed to 1 in this release, for a minor compression ratio boost. Unfortunately, this change slows decompression by around 1-2% relative to Alpha6. I hope to get this speed back by optimizing the prefix code table construction code more (by borrowing some work I did in the miniz project). I'm going to make this a compression flag in the next release. For now, if you don't like this just set this macro to 0.

I've fixed a few minor API bugs found while integrating LZHAM into the command line and GUI versions of 7-zip. I now have a customized version of 7-zip that supports LZHAM, which is great for testing, but I'm not planning on releasing it (unless there's interest).

I added initial support for large matches (>258 bytes), which are currently only used for delta compression.

The compressor can now inform the decompressor to reinitialize the update frequency of all prefix code tables back to the max frequency. This feature is only exploited when the LZHAM\_COMP\_FLAG\_TRADEOFF\_DECOMPRESSION\_RATE\_FOR\_COMP\_RATIO compression flag is enabled, because the extra table updates will slow down decompression. When enabled, the compressor tracks the compression ratio history of the last 6 blocks. If a block's ratio substantially drops relative to the previous 6, it issues a table rate reset.

Added new file: lzham\_lzcomp\_state.cpp, which contains all the "lzcompressor::state" related classes originally declared in lzham\_lzcomp\_internal.cpp. Now, all parsing and high-level control related code is in lzham\_lzcomp\_internal.cpp, and all state updating/encoding is in lzham\_lzcomp\_state.cpp.

**Alpha6 Changes**

I modified the compressor to use fixed point arithmetic to track bit prices, instead of floating point math. (The compressor should not use floating point math anywhere now.) This ensures the compressor's output doesn't vary between compilers, optimization settings, platforms, etc. The compression ratio has actually improved a tiny amount (by around .05% to .08%) in this release due to this change. (Thanks to Aaron Nicholls for emphasizing the importance of deterministic compression.)

I also removed an unnecessary per-block memcpy() from the compressor for a minor perf. savings.

Alpha6's output should be binary compatible with Alpha5 (and vice versa).

**Alpha5 Changes**

x86 Linux support: Alpha5's compressor now supports the pthreads API for threading and GCC's atomic built-in functions for lock-free operations. I've included a Codeblocks IDE workspace for Linux (lzhamtest\_linux.workspace). The codec has been tested under 32-bit Ubuntu (Lucid Lynx). There's still work to be done here: 64-bit support, proper makefiles, and much more testing is needed.

Alpha5's decompressor is now implemented purely as a coroutine. (Previous versions relied  on the Win32 Fiber API. The upside to the coroutine method is cross platform compatibility, and the downside is a definite increase in complexity.) The coroutine approach closely resembles the one outlined here: [Coroutines in C](http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html)

LZHAM's compression ratio is now closer to LZMA than alpha4, occasionally even beating it. Decompression speed has been improved by around 4-8% on Core i7 processors, and compression ratio has been improved by a tiny amount.

The compressor's load balancing efficiency on multicore CPU's has been improved, particularly when the "extreme" parser (-x option) is enabled.

The codec can now be compiled under plain vanilla ANSI C/C++ by defining the LZHAM\_ANSI\_CPLUSPLUS macro. This disables all threading and atomic operations in the compressor.

The compressed bitstream format has been cleaned up a bit. Important: Compressed bitstreams created by earlier alpha releases are not compatible with this release, and vice versa.

I've included two alternatives to Polar coding (Shannon-Fano and Fyffe) that can be enabled for experimental purposes. See the LZHAM\_USE\_SHANNON\_FANO\_CODES and LZHAM\_USE\_FYFFE\_CODES #defines in lzhamdecomp/lzham\_polar\_codes.cpp.

**Alpha4 Changes**

The compressor in alpha4 has been optimized for speed and has a slightly higher compression ratio vs. alpha3:

  * Near-optimal parser rewritten: Completely removed "unvisited" node list, more efficient cost evaluation, more effective/simpler match suppression.
  * Match finder improvements: A separate small hash table is now used for len2 match finding, allowing the main match finder to concentrate on len3+ matches.
  * Task pool class was simplified and improved by using a lock-free stack.
  * Some of the work on the main thread was moved around a bit to better increase parallelism. Also, len2 match finding is now done on the main thread in parallel with the main match finder.
  * Polar coding can now be optionally enabled in all compression levels.
  * alpha4 also includes an optional "extreme" parser that records the four lowest cost paths found to each character in the lookahead buffer. (The regular parser only records the cheapest path to each character.) This parser is enabled by using the "-x" option in the test app. (-x is probably impractically slow unless you're on a machine with a lot of cores. It partially exists to help me determine the upper limit to how much parsing changes alone can increase LZHAM's compression ratio.)

**Alpha3 Changes**

The alpha3 release greatly improves compression speed, particularly on multicore systems. alpha3's near-optimal parser includes a number of additional speed optimizations and is now multithreaded for an overall 3-4x increase in compression speed vs. alpha2 (on a Core i7 using 3 parser threads and 5 match finder threads). alpha3's compression ratio should be more or less the same as alpha2's.

The lzhamtest\_x86/x64 test executable now supports several command line parameters to control dictionary size, compression level, the number of helper threads used, etc.

The codec has been ported and minimally tested on the big endian Xbox 360 platform, and now compiles with GCC 4.5.0 using TDM-GCC x64.

The included pre-compiled executables no longer rely on the VC8 C run time library DLL's, and should run on WinXP systems.