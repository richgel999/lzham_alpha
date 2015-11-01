# LZHAM Codec Reference #

Applications can use LZHAM by either statically linking against two LIB's named lzhamdecomp.lib and lzhamcomp.lib, or by loading a DLL named lzham\_x86.dll or lzham\_x64.dll.

The DLL version supports both compression/decompression. Apps that use the LIB's and don't require compression functionality only need to link with lzhamdecomp.lib. Apps that also need compression functionality must link against both libs (even if decompression functionality is not needed) because the compressor depends on functionality present in the decompression lib. (The actual library file names are lib/x86/lzhamcomp\_x86.lib, lib/x64/lzhamcomp\_x64.lib, etc. See the `lib` directory.)

Alternately, the two lib structure present in the LZHAM distribution can be discarded. It may be more convienent to just copy all .cpp/.h files in the `include`, `lzhamcomp` and `lzhamdecomp` directories into a single flat directory and then compile/link these files directly into the user's application.

## Public API ##

The entire public API is defined in the single header file `include/lzham.h`. This is a C-style API containing functions for streaming compression/decompression, single function call compression/decompression, a function to return the codec's version, and a function that allows the user to override how the codec alllocates memory. The compression/decompression functions are somewhat similar to zlib's or LZMA's.

A set of strictly optional helper classes that simplify loading and using the DLL interface are defined in `include/lzham_dynamic_lib.h` and `include/lzham_static_lib.h`.

Currently, the only working example of the public API is in the command line test app `lzhamtest/lzhamtest.cpp`.

All private LZHAM code is placed in the `lzham` namespace. All public API's are in the global namespace and begin with the `"lzham_"` prefix.

## C++ Features ##

The codec is currently written in C++, but it only uses a subset of the full language: no exceptions, no multiple inheritance, no calls to new/delete, and very few virtuals. All memory allocation can be overriden by the user. A few templates are used, but (hopefully) in easily understood ways. (As time permits, LZHAM will be ported to C.)

## Thread Safety ##

All codec state (excluding the memory allocation callback functions, which are global pointers) is stored within user-managed compression/decompression state objects (created by `lzham_compress_init()` or `lzham_decompress_init()`), so the codec is thread safe (i.e. multiple compressors/decompressors may be active simultaneously on _different_ state objects).

However, on a single state object, the compression/decompression API's are not thread safe (i.e. the API's don't do any locking for you at the state object level). In other words, don't do something like calling `lzham_compress()` simultaneously from multiple threads on the _same_ state object - bad things will happen.


---


# Configuration #

The header file `lzhamdecomp/lzham_core.h` configures both the decompressor and compressor. The lib has been tested under Windows (x86 and x64 using VS 2008 and VS 2010), TDM GCC x86/x64, and under 32-bit Ubuntu Linux. Mac/BSD support isn't there yet, but it's on the radar.

The codec can be compiled under plain vanilla ANSI C/C++ by defining the `LZHAM_ANSI_CPLUSPLUS` macro in `lzham_core.h`. This shouldn't have a big impact on decompression, but this unfortunately disables all threading and atomic operations in the compressor.


---


# API Categories #

## Versioning ##

```
   lzham_uint32 lzham_get_version(void);
```

Returns the codec's version. The high byte contains the major version (currently 0x10), and the low byte contains the minor version (0x07 in the alpha7 release).

## Custom Memory Allocation Callbacks ##

```
   #define LZHAM_MIN_ALLOC_ALIGNMENT sizeof(size_t) * 2

   typedef void*  (*lzham_realloc_func)(void* p, size_t size, size_t* pActual_size, bool movable, void* pUser_data);
   typedef size_t (*lzham_msize_func)(void* p, void* pUser_data);

   void lzham_set_memory_callbacks(lzham_realloc_func pRealloc, lzham_msize_func pMSize, void* pUser_data);
```

By default, the codec calls the usual C-API's to manage memory (`malloc`, `realloc`, `free`, etc.). Call `lzham_set_memory_callbacks` to globally override this behavior. The user must implement two callbacks, one to handle block allocation/reallocation/freeing, and another that returns the size of allocated blocks.

Avoid calling this function if there are any currently active compression/decompression state objects. This function is not thread safe.

The custom realloc and msize functions must be implemented in a thread safe manner. These functions can be called from multiple threads when threaded compression is enabled.

All block pointers returned by the realloc callback must be aligned to at least `LZHAM_MIN_ALLOC_ALIGNMENT` bytes.

### realloc callback ###

The custom reallocation function callback `lzham_realloc_func` must examine its input parameters to determine the caller's actual intent. If the input pointer `p` is NULL, the caller wants to allocate a block which must be at least as large as `size`. NULL is returned if the allocation fails.

If `p` is not NULL but `size` is 0, the caller wants to free the block pointed to by `p`.

Otherwise, the caller wants to attempt to change the size of the block pointed to by `p`. In this case, if `movable` is true, it is acceptable to physically move the block to satisfy the reallocation request. If `movable` is false, the block **must not** be moved. NULL is returned if reallocation fails for any reason. In this case, the original allocated block must remain allocated.

If `pActual_size` is not NULL, `*pActual_size` should be set to the actual size of the returned block.

For an example implemention, see the function `lzham_default_realloc` in lzhamdecomp/lzham\_mem.cpp.

### msize callback ###

The custom memory size callback `lzham_msize_func` must return the actual size of the allocated block pointed to by `p`, or 0 if `p` is NULL. The default implementation in lzhamdecomp/lzham\_mem.cpp just calls `_msize`.

## Compression ##

### Important Macros ###
```
   #define LZHAM_MIN_DICT_SIZE_LOG2 15
   #define LZHAM_MAX_DICT_SIZE_LOG2_X86 26
   #define LZHAM_MAX_DICT_SIZE_LOG2_X64 29
```

Compression requires at least dict\_size+65536+dict\_size\*4\*2+65536\*4 bytes of available heap memory. Under x86, the maximum supported dictionary size is 2<sup>26</sup> (64MB), and under x64 the maximum is currently 2<sup>29</sup> (512MB). The minimum supported dictionary size is 32KB, but note the codec hasn't really been tuned for max throughput or coding efficiency with tiny dictionary sizes.

```
   #define LZHAM_MAX_HELPER_THREADS 16
```

The compressor can optionally create up to LZHAM\_MAX\_HELPER\_THREADS "helper" threads to assist in compression. These helper threads can be used to accelerate parsing and match finding. Note, the original calling thread is NOT included in this total. For example, if the # of helper threads is 4, up to 5 threads total will be involved in compression (the calling thread plus 4 additional threads). Decompression is always single threaded.

### Compression Related Enums ###
#### lzham\_compress\_status\_t ####

```
   enum lzham_compress_status_t
   {
      LZHAM_COMP_STATUS_NOT_FINISHED = 0,
      LZHAM_COMP_STATUS_NEEDS_MORE_INPUT,

      // All the following enums must indicate failure/success.
      LZHAM_COMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE,

      LZHAM_COMP_STATUS_SUCCESS = LZHAM_COMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE,
      LZHAM_COMP_STATUS_FAILED,
      LZHAM_COMP_STATUS_FAILED_INITIALIZING,
      LZHAM_COMP_STATUS_INVALID_PARAMETER,
      LZHAM_COMP_STATUS_OUTPUT_BUF_TOO_SMALL
   };
```

`lzham_compress_status_t` defines the possible return status codes from the compressor. The first set of codes (NOT\_FINISHED and NEEDS\_MORE\_INPUT) indicate that the compressor can't continue because the input buffer is empty, and/or the output buffer is full. The rest of the codes indicate either a success or failure condition.

#### lzham\_compress\_level ####
```
   enum lzham_compress_level
   {
      LZHAM_COMP_LEVEL_FASTEST = 0,
      LZHAM_COMP_LEVEL_FASTER,
      LZHAM_COMP_LEVEL_DEFAULT,
      LZHAM_COMP_LEVEL_BETTER,
      LZHAM_COMP_LEVEL_UBER,

      LZHAM_TOTAL_COMP_LEVELS
   };
```

`lzham_compress_level` allows the caller to control the tradeoff between compression ratio and throughput. `LZHAM_COMP_LEVEL_FASTEST` is the fastest level with the lowest compression, and `LZHAM_COMP_LEVEL_UBER` is the slowest level with highest compression.

#### lzham\_compress\_flags ####
```
   enum lzham_compress_flags
   {
      LZHAM_COMP_FLAG_FORCE_POLAR_CODING = 1,      
      LZHAM_COMP_FLAG_EXTREME_PARSING = 2,         
      LZHAM_COMP_FLAG_DETERMINISTIC_PARSING = 4,   
      LZHAM_COMP_FLAG_TRADEOFF_DECOMPRESSION_RATE_FOR_COMP_RATIO = 16,
   };
```


`lzham_compress_flags` define the various flags that may be logically OR'd together:

`LZHAM_COMP_FLAG_FORCE_POLAR_CODING`: Forces Polar codes vs. Huffman, for a slight increase in decompression speed.

`LZHAM_COMP_FLAG_EXTREME_PARSING`: Improves ratio by allowing the compressor's parse graph to grow "higher" (up to 4 parent nodes per output node), but is much slower.

`LZHAM_COMP_FLAG_DETERMINISTIC_PARSING`: Guarantees that the compressed output will always be the same given the same input data, the same compression parameters, and the same build. Otherwise, the compressed output stream may vary a small amount due to kernel threading scheduling differences between runs.

`LZHAM_COMP_FLAG_TRADEOFF_DECOMPRESSION_RATE_FOR_COMP_RATIO`:
If enabled, the compressor is free to use any optimizations which could lower the decompression throughput rate but improve ratio. For example, the compressor may adaptively reset the Huffman table update rate to maximum frequency if doing so would improve ratio, which is expensive for the decompressor.

### Compression Parameters Struct ###

```
   struct lzham_compress_params
   {
      lzham_uint32 m_struct_size;            
      lzham_uint32 m_dict_size_log2;         
      lzham_compress_level m_level;          
      lzham_uint32 m_max_helper_threads;     
      lzham_uint32 m_cpucache_total_lines;   
      lzham_uint32 m_cpucache_line_size;     
      lzham_uint32 m_compress_flags;         
      lzham_uint32 m_num_seed_bytes;         
      const void *m_pSeed_bytes;
   };
```

`lzham_compress_params` contains various initialization parameters. The caller should clear this structure to all-0's, set `m_struct_size` to sizeof(lzham\_compress\_params}, then fill in these parameters:

`m_dict_size_log2`: Set to the log2(dictionary\_size), must range between [`LZHAM_MIN_DICT_SIZE_LOG2`, `LZHAM_MAX_DICT_SIZE_LOG2_X86`] for x86 or `LZHAM_MAX_DICT_SIZE_LOG2_X64` for x64. See [Important Macros](API_Docs#Important_Macros.md).

`m_level`: Compression level. Set to LZHAM\_COMP\_LEVEL\_FASTEST, etc. See the [lzham\_compress\_level](API_Docs#lzham_compress_level.md) enum.

`m_max_helper_threads`: Maximum # of additional "helper" threads to create, must range between [0,`LZHAM_MAX_HELPER_THREADS`]. See [Important Macros](API_Docs#Important_Macros.md).

`m_cpucache_total_lines`: Set to 0 (optimize compressed stream to avoid L1/L2 cache misses - not currently supported)

`m_cpucache_line_size`: Set to 0

`m_compress_flags`: Optional compression flags (see lzham\_compress\_flags enum)

`m_num_seed_bytes`: Optional, for delta compression. Set to the number of seed bytes pointed to by m\_pSeed\_bytes. This value must be less than or equal to the size of the dictionary size.

`m_pSeed_bytes`: Optional, for delta compression. Pointer to seed bytes buffer. Buffer must be at least m\_num\_seed\_bytes long.

### Compression Functions ###

#### lzham\_compress\_init ####
```
   lzham_compress_state_ptr lzham_compress_init(const lzham_compress_params *pParams);
```

`lzham_compress_init` initializes a compressor. Returns a pointer to the compressor's internal state object, or NULL on failure. pParams cannot be NULL. Be sure to initialize the pParams->m\_struct\_size member to sizeof(lzham\_compress\_params) (along with the other members to reasonable values) before calling this function. Multiple state objects may be created.

#### lzham\_compress\_deinit ####
```
   lzham_uint32 lzham_compress_deinit(lzham_compress_state_ptr pState);
```

Deinitializes a compressor, releasing all allocated memory. Returns adler32 of source data (valid only on success).

#### lzham\_compress ####
```
   lzham_compress_status_t lzham_compress(
      lzham_compress_state_ptr pState,
      const lzham_uint8 *pIn_buf, size_t *pIn_buf_size,
      lzham_uint8 *pOut_buf, size_t *pOut_buf_size,
      lzham_bool no_more_input_bytes_flag);
```

Compresses an arbitrarily sized block of data, writing as much available compressed data as possible to the output buffer.
This method may be called as many times as needed (it behaves like a coroutine), but for best perf. try not to call it with tiny buffers.

`pState`: Pointer to internal compression state, created by `lzham_compress_init`.

`pIn_buf`, `pIn_buf_size`: Pointer to input data buffer, and pointer to a size\_t containing the number of bytes available in this buffer. On return, `*`pIn\_buf\_size will be set to the number of bytes actually read from the buffer, which may be 0.

`pOut_buf`, `pOut_buf_size`: Pointer to the output data buffer, and a pointer to a size\_t containing the max number of bytes that can be written to this buffer. On return, `*`pOut\_buf\_size will be set to the number of bytes written to this buffer, which may be 0.

`no_more_input_bytes_flag`: Set to true to indicate that no more input bytes are available to compress (EOF). Once you call this function with this param set to true, it must stay set to true in all future calls.

#### Normal return status codes: ####

`LZHAM_COMP_STATUS_NOT_FINISHED`: Compression can continue, but the compressor needs more input, or it needs more room in the output buffer.

`LZHAM_COMP_STATUS_NEEDS_MORE_INPUT`: Compression can continue, but the compressor has no more output, and has no input but the caller has not indicated that we're at EOF. Supply more input to continue.

#### Success/failure return status codes: ####

`LZHAM_COMP_STATUS_SUCCESS`: Compression has completed successfully.

`LZHAM_COMP_STATUS_FAILED`, `LZHAM_COMP_STATUS_FAILED_INITIALIZING`, `LZHAM_COMP_STATUS_INVALID_PARAMETER`: Something went wrong.

#### lzham\_compress\_memory ####
```
  lzham_compress_status_t lzham_compress_memory(
      const lzham_compress_params *pParams,
      lzham_uint8* pDst_buf,
      size_t *pDst_len,
      const lzham_uint8* pSrc_buf,
      size_t src_len,
      lzham_uint32 *pAdler32);
```

Compresses source data from pSrc\_buf of size src\_len to pDst\_buf in a single call. On entry, `*`pDst\_len must be the size of the destination buffer. On exit, `*`pDst\_len will contain the number of bytes written to the destination buffer. `*`pAdler32 will be set to the alder32 of the source data. Same return codes as `lzham_compress`, except this function can also return LZHAM\_COMP\_STATUS\_OUTPUT\_BUF\_TOO\_SMALL.

## Decompression ##
### Decompression Enums ###
#### lzham\_decompress\_status\_t ####
```
   enum lzham_decompress_status_t
   {
      LZHAM_DECOMP_STATUS_NOT_FINISHED = 0,
      LZHAM_DECOMP_STATUS_NEEDS_MORE_INPUT,

      // All the following enums always (and MUST) indicate failure/success.
      LZHAM_DECOMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE,

      LZHAM_DECOMP_STATUS_SUCCESS = LZHAM_DECOMP_STATUS_FIRST_SUCCESS_OR_FAILURE_CODE,

      LZHAM_DECOMP_STATUS_FIRST_FAILURE_CODE,

      LZHAM_DECOMP_STATUS_FAILED_INITIALIZING = LZHAM_DECOMP_STATUS_FIRST_FAILURE_CODE,
      LZHAM_DECOMP_STATUS_FAILED_DEST_BUF_TOO_SMALL,
      LZHAM_DECOMP_STATUS_FAILED_HAVE_MORE_OUTPUT,
      LZHAM_DECOMP_STATUS_FAILED_EXPECTED_MORE_RAW_BYTES,
      LZHAM_DECOMP_STATUS_FAILED_BAD_CODE,
      LZHAM_DECOMP_STATUS_FAILED_ADLER32,
      LZHAM_DECOMP_STATUS_FAILED_BAD_RAW_BLOCK,
      LZHAM_DECOMP_STATUS_FAILED_BAD_COMP_BLOCK_SYNC_CHECK,
      LZHAM_DECOMP_STATUS_INVALID_PARAMETER,
   };
```

`lzham_decompress_status_t` defines the possible return status codes from the decompressor. The first set of codes (NOT\_FINISHED and NEEDS\_MORE\_INPUT) indicate that the decompressor can't continue because the input buffer is empty, and/or the output buffer is full. The rest of the codes indicate either a success or failure condition.

### Decompression Parameters Struct ###
#### lzham\_decompress\_params ####
```
   struct lzham_decompress_params
   {
      lzham_uint32 m_struct_size;            
      lzham_uint32 m_dict_size_log2;         
      lzham_bool m_output_unbuffered;        
      lzham_bool m_compute_adler32;          
      lzham_uint32 m_num_seed_bytes;         
      const void *m_pSeed_bytes;        
   };
```

`lzham_decompress_params` contains various initialization parameters. The caller should clear this structure to all-0's, set `m_struct_size` to sizeof(lzham\_decompress\_params}, then fill in these parameters:

`m_struct_size`: Set to sizeof(lzham\_decompress\_params)

`m_dict_size_log2`: Set to the log2(dictionary\_size), must range between [LZHAM\_MIN\_DICT\_SIZE\_LOG2, LZHAM\_MAX\_DICT\_SIZE\_LOG2\_X86] for x86 LZHAM\_MAX\_DICT\_SIZE\_LOG2\_X64 for x64. See [Important Macros](API_Docs#Important_Macros.md).

`m_output_unbuffered`: Set to true if the output buffer is guaranteed to be large enough to hold the entire output stream (a bit faster)

`m_compute_adler32`: Set to true to enable adler32 checking during decompression (slightly slower)

`m_num_seed_bytes`: Optional, for delta compression - number of seed bytes pointed to by m\_pSeed\_bytes

`m_pSeed_bytes`: Optional, for delta compression - pointer to seed bytes buffer, must be at least m\_num\_seed\_bytes long

### Decompression Functions ###

#### lzham\_decompress\_init ####
```
   lzham_decompress_state_ptr lzham_decompress_init(const lzham_decompress_params *pParams);
```

Initializes a decompressor. pParams cannot be NULL. Be sure to initialize the pParams->m\_struct\_size member to sizeof(lzham\_decompress\_params) (along with the other members to reasonable values) before calling this function.

Note: With large dictionaries this function could take a while (due to memory allocation). To decompress multiple independent streams, it's faster to init a decompressor state object just one time, use it, then reuse it multiple times as needed by calling `lzham_decompress_reinit()`.

#### lzham\_decompress\_reinit ####
```
   lzham_decompress_state_ptr lzham_decompress_reinit(lzham_decompress_state_ptr pState, 
      const lzham_decompress_params *pParams);
```

Quickly re-initializes the decompressor state object to its initial state (avoiding any memory allocation/freeing unless absolutely necessary).

#### lzham\_decompress\_deinit ####
```
   lzham_uint32 lzham_decompress_deinit(lzham_decompress_state_ptr pState);
```

Deinitializes a decompressor, freeing any allocated memory associated with the state object. Returns adler32 of decompressed data if the `compute_adler32` decompression parameter was true, otherwise it returns the adler32 of the source (uncompressed) data that was read from the compressed data stream.

#### lzham\_decompress ####
```
   lzham_decompress_status_t lzham_decompress(
      lzham_decompress_state_ptr pState,
      const lzham_uint8 *pIn_buf, size_t *pIn_buf_size,
      lzham_uint8 *pOut_buf, size_t *pOut_buf_size,
      lzham_bool no_more_input_bytes_flag);
```

Decompresses an arbitrarily sized block of compressed data, writing as much available decompressed data as possible to the output buffer. This method is implemented as a coroutine so it may be called as many times as needed. However, for best perf. try not to call it with tiny buffers.

`pState`: Pointer to internal decompression state, originally created by `lzham_decompress_init`.

`pIn_buf`, `pIn_buf_size`: Pointer to input data buffer, and a pointer to a size\_t containing the number of bytes available in this buffer. On return, `*`pIn\_buf\_size will be set to the number of bytes actually read from the buffer, which may be 0.

`pOut_buf`, `pOut_buf_size`: Pointer to the output data buffer, and a pointer to a size\_t containing the max number of bytes that can be written to this buffer. On return, `*`pOut\_buf\_size will be set to the number of bytes written to this buffer, which may be 0.

`no_more_input_bytes_flag`: Set to true to indicate that no more input bytes are available to compress (EOF). Once you call this function with this param set to true, it must remain set to true in all future calls.

Notes:

In unbuffered mode, the output buffer **must** be large enough to hold the entire decompressed stream. Otherwise, you'll receive the `LZHAM_DECOMP_STATUS_FAILED_DEST_BUF_TOO_SMALL` error (which is currently unrecoverable during unbuffered decompression).

In buffered mode, if the caller indicates the output buffer's size is 0 bytes, the caller is indicating that no more output bytes are expected from the decompressor. In this case, if the decompressor actually has more bytes available you'll receive the `LZHAM_DECOMP_STATUS_FAILED_HAVE_MORE_OUTPUT` error (which is recoverable in the buffered case - just call `lzham_decompress()` again with a non-zero size output buffer).

#### lzham\_decompress\_memory ####
```
   lzham_decompress_status_t lzham_decompress_memory(
      const lzham_decompress_params *pParams,
      lzham_uint8* pDst_buf,
      size_t *pDst_len,
      const lzham_uint8* pSrc_buf,
      size_t src_len,
      lzham_uint32 *pAdler32);
```

Decompresses source data from pSrc\_buf of size src\_len to pDst\_buf in a single call. On entry, `*`pDst\_len must be set to the size of the destination buffer. On exit, `*`pDst\_len will be set to the actual number of bytes written to the destination buffer. `*`pAdler32 will be set to the alder32 of the decompressed data. Same return codes as `lzham_decompress`, except this function can also return `LZHAM_DECOMP_STATUS_FAILED_DEST_BUF_TOO_SMALL`.