Updated 9/1/10

LZHAM is now listed on the Large Text Compression Benchmark page:
http://mattmahoney.net/dc/text.html

Interestingly, once you sort this list by decompression speed, LZHAM is among the top 20 fastest decompressors listed. Of those 20, it has the highest compression ratio on enwik9, and the 2nd best on enwik8. (The closed source "lzturbo" codec has a slightly higher compression ratio on enwik8, and decompresses at 20ns/byte vs. LZHAM's 21ns/byte when the codec was tested on similar machines.)

Here are the results from the alpha3 x64 version of the compressor using a 512MB dictionary in "Uber" mode (using the -d29 -m4 command line options):

  * LZHAM x64 alpha3 enwik8: Input file size: 100,000,000, Compressed file size: 24,954,329
  * LZHAM x64 alpha3 enwik9: Input file size: 1,000,000,000, Compressed file size: 206,393,809

For comparison, here are lzturbo and 7zip's compressed file sizes::

  * lzturbo 0.94 enwik8: 24,763,542
  * lzturbo 0.94 enwik9: 217,342,694

7-zip 4.65 command line (7za) 4.65 statistics:
  * 7za -mx9 enwik8: 24,861,205
  * 7za -mx9 enwik9: 213,336,006

7-zip 9.09 GUI x64 using 512MB dictionary/ultra:
  * 7zip x64 512MB: enwik8: 24,797,848
  * 7zip x64 512MB: enwik9: 202,900,157

Here's the output from the lzhamtest\_x64 running under Win7 x64 on a 2.6 GHz Core i7 with 6GB RAM:

```
***** [16 of 48] Compressing file "E:\dev\corpus\enwik8\enwik8" to "__comp_temp_216544519__.tmp"
Testing: Streaming compression
lzham_compress_init took 3.578ms
Success
Input file size: 100000000, Compressed file size: 24954329, Ratio: 75.05%
Compression time: 48.284000
Consumption rate: 2071079.4 bytes/sec, Emission rate:  516824.0 bytes/sec
Input file adler32: 0x72DB5861
Decompressing file "__comp_temp_216544519__.tmp" to "__decomp_temp_216544519__.tmp"
Testing: Streaming decompression
lzham_decompress_init took 0.020ms
Success
Source file size: 24954329, Decompressed file size: 100000000
Decompressed adler32: 0x72DB5861
Overall decompression time (decompression init+I/O+decompression): 1.040657
  Consumption rate: 23979403.1 bytes/sec, Decompression rate: 96093159.3 bytes/sec
Decompression only time (not counting decompression init or I/O): 0.991624
  Consumption rate: 25165113.4 bytes/sec, Decompression rate: 100844680.8 bytes/sec

***** [17 of 48] Compressing file "E:\dev\corpus\enwik9\enwik9" to "__comp_temp_216544519__.tmp"
Testing: Streaming compression
lzham_compress_init took 3.703ms
Success
Input file size: 1000000000, Compressed file size: 206393809, Ratio: 79.36%
Compression time: 595.085065
Consumption rate: 1680432.0 bytes/sec, Emission rate:  346830.8 bytes/sec
Input file adler32: 0x55B40785
Decompressing file "__comp_temp_216544519__.tmp" to "__decomp_temp_216544519__.tmp"
Testing: Streaming decompression
lzham_decompress_init took 0.027ms
Success
Source file size: 206393809, Decompressed file size: 1000000000
Decompressed adler32: 0x55B40785
Overall decompression time (decompression init+I/O+decompression): 9.242904
  Consumption rate: 22329974.8 bytes/sec, Decompression rate: 108191107.7 bytes/sec
Decompression only time (not counting decompression init or I/O): 8.798123
  Consumption rate: 23458845.1 bytes/sec, Decompression rate: 113660604.6 bytes/sec
```

**Compression Speed**

LZHAM's compressor in "uber" mode is now reasonably fast on modern multicore processors (around 1-3MB/sec. depending on the file), but it still uses relatively more total CPU time vs. LZMA. This is partially on purpose, because LZHAM tries to make up for its relatively weaker (but faster to decode) coding scheme vs. LZMA's all-arithmetic coder by using good parsing and higher order context modeling. But fundamentally, LZMA's parser efficiency is very impressive.

Example: LZHAM alpha3 x64 took 48.28 seconds to compress enwik8, but it used 8 total threads to do so. LZHAM x86 took 55.34 seconds (also using 8 total threads). By comparison, as far as I know 7za (7zip command line) only uses a single helper thread and took 69.15 seconds to compress the same file:

7za enwik8:

```
E:\lzham\lzham_alpha3_head2\bin>timer 7za a -mx9 d E:\dev\corpus\enwik8\enwik8

Timer 8.00 : Igor Pavlov : Public domain : 2008-11-25

7-Zip (A) 4.65  Copyright (c) 1999-2009 Igor Pavlov  2009-02-03
Scanning

Creating archive d.7z

Compressing  enwik8

Everything is Ok

Kernel Time  =     0.561 = 00:00:00.561 =   0%
User Time    =   100.995 = 00:01:40.995 = 146%
Process Time =   101.556 = 00:01:41.556 = 146%
Global Time  =    69.151 = 00:01:09.151 = 100%
```

For an apples to apples comparison, here's lzhamtest\_x86 compressing the same file. LZHAM only took 55.4s vs. 7za's 69.1s, but it used 3x as much total CPU time (this was run on a Core i7, which supports 8 total hardware threads, on 4 cores with 2 hyperthreads per core):

lzhamtest\_x86:

```
E:\lzham\lzham_alpha3_head2\bin>timer lzhamtest_x86 c E:\dev\corpus\enwik8\enwik8 1

Timer 8.00 : Igor Pavlov : Public domain : 2008-11-25
LZHAM Codec - x86 Command Line Test App - Compiled Aug 30 2010 01:57:59
Expecting LZHAM DLL Version 0x1003
Dynamically loading DLL "E:\lzham\lzham_alpha3_head2\bin\lzham_x86.dll"
Loaded LZHAM DLL version 0x1003

Using options:
Comp level: 4
Dict size: 26 (67108864 bytes)
Compute adler32 during decompression: 1
Max helper threads: 7
Unbuffered decompression: 0
Verify compressed data: 0
Randomize parameters: 0

Testing: Streaming compression
lzham_compress_init took 7.334ms
Success
Input file size: 100000000, Compressed file size: 25013133, Ratio: 74.99%
Compression time: 55.343016
Consumption rate: 1806912.7 bytes/sec, Emission rate:  451965.5 bytes/sec
Input file adler32: 0x72DB5861

Kernel Time  =     0.592 = 00:00:00.592 =   1%
User Time    =   319.599 = 00:05:19.599 = 576%
Process Time =   320.192 = 00:05:20.192 = 577%
Global Time  =    55.444 = 00:00:55.444 = 100%
```

The current in development version of lzham (alpha4) has a more optimized (and simpler) near-optimal parser. At this point, the bottlenecks are match finding and load balancing efficiency issues, not parsing.