# Introduction #

LZHAM is the result of several years of research and experience in the field of **practical** lossless data compression techniques. My previous major lossless codec shipped in several popular PC/Xbox 360 titles: Age of Empires 1/2, Halo Wars, Halo 3 and its sequels, and Forza 2.

# Details on the Compression Methods Used in LZHAM #

  * Binary arithmetic coding is used for all key coding decisions, but (unlike LZMA) literal/match symbols are coded using fast semi-adaptive canonical Polar or Huffman coding. (LZMA uses binary arithmetic coding for all symbols, making it particularly slow on console platforms with unpipelined multiply instructions.)
> Polar coding is used in the LZHAM\_COMP\_LEVEL\_FASTEST, FASTER, and DEFAULT compression levels. The overall decompression rate is approx. 12% faster with Polar codes on x86 platforms. By default, Huffman codes are used in the BETTER and UBER compression levels for a very slight (approx. .1%) increase in compression ratio but with slower decompression. (The caller can override these defaults during compression.)
> Polar coding links:
    * http://www.ezcodesample.com/prefixer/prefixer_article.html

> Another fast non-optimal prefix code construction algorithm was created by Graham Fyffe in 1999:
    * http://code.google.com/p/lzham/wiki/FyffeCodes

  * Multithreaded forward near-optimal parsing: Internally, the compressor works on 512KB blocks at a time. Each block is subdivided into non-overlapping 3KB sections. Each section is parsed in parallel using a streamlined/simplified version of Dijkstra's algorithm starting from the same coding state, which remains mostly static during parsing. (Symbol statistics are not updated during parsing, but the state machine and match history buffer are accurately updated.) Once all parallel parse jobs are completed the main thread codes the results using the true coding state.

> alpha3 supports up to 4 simultaneous parsers - 1 running on the main thread, and 3 running on helper threads.

> The latest alpha version of the compressor also supports an "extreme" optimal parser, which tracks up to three solutions (and bitprices) per lookahead character. This allows the compressor to make locally suboptimal decisions as long as the end result is cheaper.

> The current parsers use frozen symbol statistics during parsing, but I'm planning on investigating how difficult it would be to use fully accurate statistics in each step of the parse.

  * Multithreaded lock-free match finder: LZHAM's current search acceleration data structure is fairly basic but is designed to be efficiently updated in parallel with parsing and coding. It uses a 64K (256^2) entry hash table, where each hash table entry contains a dictionary index to the root of a unbalanced binary tree of dictionary strings. The strings within each tree begin with the same 2 byte sequence (digram). There are left/right indices associated with each dictionary entry. New strings are inserted starting at the root of the proper tree, and matches are determined during insertion. The worse case depth of each tree is limited by compressor settings.

> Before parsing, the main thread scans each 512KB block to determine all unique character digrams present in the block. The list of unique digrams is equally subdivided and distributed to the finder threads. Each digram (and binary tree) is associated with only a single helper thread, eliminating any need for synchronization while inserting strings and finding matches.

> Each match finder thread exhaustively finds a list of roughly sorted matches for every dictionary entry that begins with a digram assigned to that thread. The match lists are atomically concatenated to the end of a large match list buffer, and pointers to the beginning of each match list in this buffer are set atomically in a match reference buffer. Once the match list pointer is atomically set a parser thread can immediately consume the list in a lock-free manner.

> The parser threads consume the output from the finder threads. Hopefully, the finder threads always "stay ahead" of the parsers. If not, the parsers back off by spinning and eventually sleeping for a bit to give the finder threads a chance to catch up and get ahead.

> This approach is not perfect - load balancing the coding, parsing, and finder threads is something I plan on improving. The current compressor has ~80% CPU utilization on a Core i7 for most files using a main thread plus 2 parser threads, and 5 finder threads (8 total threads). Currently, parsing and coding cannot be overlapped, but match finding always runs in parallel with other activities.

  * Uses the same basic state machine as LZMA, but most symbols are coding using higher order context modeling. Literal/delta literals and rep/history matches are also coded similarly, except LZHAM uses partial order-2 modeling of literals, partial order-1 modeling of the "is match" bit.

  * The least significant 4 bits of match distances are coded using a separately modeled symbol, and the exhaustive match finder tends to favor matches with the lowest value in the least significant 4 bits of the match distance.

  * Canonical Huffman prefix codes are generated using radix sorting combined with Moffat's linear time algorithm to compute symbol codelengths. The radix sorter's inner loop takes steps to decrease Load-Hit-Stores (LHS's) on console platforms.

  * Canonical prefix code decoder uses ideas from the JPEG and Deflate standards, along with the basic decoding approach described here:
> ["On the Implementation of Minimum Redundancy Prefix Codes", Moffat and Turpin, 1997.](http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.101.6610)

  * Decoder supports 32 or 64 bit bitbuffer sizes, and contains two versions of the arith/prefix code decoders for 32 and 64-bit platforms. Overall, the x64 decompressor is 20-24% faster than the x86 decompressor on a Core i7.

  * LZHAM's symbol codec class borrows a few ideas from Amir Said's FastAC library: http://www.cipr.rpi.edu/~said/FastAC.html