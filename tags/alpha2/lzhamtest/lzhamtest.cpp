// File: lzhamtest.cpp
//
// Copyright (c) 2009-2010 Richard Geldreich, Jr. <richgel99@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <memory.h>
#include <vector>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define LZHAM_DECLARE_DYNAMIC_DLL_LOADER
#include "lzham.h"

#ifdef _DEBUG
const bool g_is_debug = true;
#else
const bool g_is_debug = false;
#endif

typedef unsigned char uint8;
typedef unsigned int uint;
typedef unsigned int uint32;
typedef __int64 int64;
typedef unsigned __int64 uint64;

#define DICT_SIZE_LOG2 LZHAM_MAX_DICT_SIZE_LOG2_X86
//#define DICT_SIZE_LOG2 LZHAM_MAX_DICT_SIZE_LOG2_X64

#define COMPRESSION_LEVEL LZHAM_COMP_LEVEL_DEFAULT

// CPU L2/L3 cache simulation has not been tested/refined much yet, so don't use it.
#define CPUCACHE_SIM_SIZE 0

int simple_test(lzham_dll_loader &lzham_dll)
{
   printf("\n");
   printf("LZHAM simple memory to memory compression test\n");
         
   lzham_compress_params comp_params;
   memset(&comp_params, 0, sizeof(comp_params));
   comp_params.m_struct_size = sizeof(comp_params);
   comp_params.m_dict_size_log2 = DICT_SIZE_LOG2;
   comp_params.m_level = LZHAM_COMP_LEVEL_FASTEST;
   comp_params.m_max_helper_threads = 0;
      
   lzham_uint8 cmp_buf[1024];
   size_t cmp_len = sizeof(cmp_buf);
   
   const char *p = "This is a test.This is a test.This is a test.1234567This is a test.This is a test.123456";
   size_t uncomp_len = strlen(p);
   
   lzham_uint32 comp_adler32 = 0;
   lzham_compress_status_t comp_status = lzham_dll.lzham_compress_memory(&comp_params, cmp_buf, &cmp_len, (const lzham_uint8 *)p, uncomp_len, &comp_adler32);
   if (comp_status != LZHAM_COMP_STATUS_SUCCESS)
   {
      printf("Compression test failed with status %i!\n", comp_status);
      return EXIT_FAILURE;
   }
   
   printf("Uncompressed size: %u\nCompressed size: %u\n", uncomp_len, cmp_len);
   
   lzham_decompress_params decomp_params;
   memset(&decomp_params, 0, sizeof(decomp_params));
   decomp_params.m_struct_size = sizeof(decomp_params);
   decomp_params.m_dict_size_log2 = DICT_SIZE_LOG2;
   decomp_params.m_compute_adler32 = true;
   
   lzham_uint8 decomp_buf[1024];
   size_t decomp_size = sizeof(decomp_buf);
   lzham_uint32 decomp_adler32 = 0;
   lzham_decompress_status_t decomp_status = lzham_dll.lzham_decompress_memory(&decomp_params, decomp_buf, &decomp_size, cmp_buf, cmp_len, &decomp_adler32);
   if (decomp_status != LZHAM_COMP_STATUS_SUCCESS)
   {
      printf("Compression test failed with status %i!\n", decomp_status);
      return EXIT_FAILURE;
   }
   
   if ((comp_adler32 != decomp_adler32) || (decomp_size != uncomp_len) || (memcmp(decomp_buf, p, uncomp_len)))
   {
      printf("Compression test failed!\n");
      return EXIT_FAILURE;
   }
   
   lzham_dll.unload();
   
   printf("Compression test succeeded.\n");

   return EXIT_SUCCESS;
}

static int compress_streaming(lzham_dll_loader &lzham_dll, const char* pSrc_filename, const char *pDst_filename, uint max_helper_threads)
{
   printf("Testing: Streaming compression\n");

   FILE *pInFile = NULL;
   FILE *pOutFile = NULL;

   fopen_s(&pInFile, pSrc_filename, "rb");
   if (!pInFile)
   {
      printf("Unable to read file: %s\n", pSrc_filename);
      return FALSE;
   }
   
   fopen_s(&pOutFile, pDst_filename, "wb");
   if (!pOutFile)
   {
      printf("Unable to create file: %s\n", pDst_filename);
      return FALSE;
   }   
   
   _fseeki64(pInFile, 0, SEEK_END);
   uint64 src_file_size = _ftelli64(pInFile);
   _fseeki64(pInFile, 0, SEEK_SET);
   
   for (uint i = 0; i < 8; i++)
   {
      fputc(static_cast<int>((src_file_size >> (i * 8)) & 0xFF), pOutFile);
   }

   const uint cInBufSize = 65536*4;
   const uint cOutBufSize = 65536*4;

   uint8 in_file_buf[cInBufSize];
   uint8 *out_file_buf = static_cast<uint8*>(_aligned_malloc(cOutBufSize, 16));

   uint64 src_bytes_left = src_file_size;

   uint in_file_buf_size = 0;
   uint in_file_buf_ofs = 0;
   
   uint64 total_output_bytes = 0;

   uint64 start_time = GetTickCount64();
   
   lzham_compress_params params;
   memset(&params, 0, sizeof(params));
   params.m_struct_size = sizeof(lzham_compress_params);
   params.m_dict_size_log2 = DICT_SIZE_LOG2;
   params.m_max_helper_threads = max_helper_threads;
   params.m_level = COMPRESSION_LEVEL;
   params.m_cpucache_line_size = 64;
   params.m_cpucache_total_lines = CPUCACHE_SIM_SIZE / params.m_cpucache_line_size;
   
   lzham_compress_state_ptr pComp_state = lzham_dll.lzham_compress_init(&params);
   if (!pComp_state)
   {
      printf("Failed initializing compressor!\n");
      _aligned_free(out_file_buf);
      fclose(pInFile);
      fclose(pOutFile);
      return FALSE;
   }

   lzham_compress_status_t status;
   for ( ; ; )
   {
      printf("Progress: %3.1f%%, Bytes remaining: %I64u\n", (1.0f - (static_cast<float>(src_bytes_left) / src_file_size)) * 100.0f, src_bytes_left);

      if (in_file_buf_ofs == in_file_buf_size)
      {
         in_file_buf_size = static_cast<uint>(min(cInBufSize, src_bytes_left));

         if (fread(in_file_buf, 1, in_file_buf_size, pInFile) != in_file_buf_size)
         {
            printf("Failure reading from source file!\n");
            _aligned_free(out_file_buf);
            fclose(pInFile);
            fclose(pOutFile);
            lzham_dll.lzham_decompress_deinit(pComp_state);
            return FALSE;
         }

         src_bytes_left -= in_file_buf_size;

         in_file_buf_ofs = 0;
      }

      uint8 *pIn_bytes = &in_file_buf[in_file_buf_ofs];
      size_t num_in_bytes = in_file_buf_size - in_file_buf_ofs;
      uint8* pOut_bytes = out_file_buf;
      size_t out_num_bytes = cOutBufSize;

      status = lzham_dll.lzham_compress(pComp_state, pIn_bytes, &num_in_bytes, pOut_bytes, &out_num_bytes, src_bytes_left == 0);

      if (num_in_bytes)
      {
         in_file_buf_ofs += (uint)num_in_bytes;
         assert(in_file_buf_ofs <= in_file_buf_size);
      }

      if (out_num_bytes)
      {
         if (fwrite(out_file_buf, 1, static_cast<uint>(out_num_bytes), pOutFile) != out_num_bytes)
         {
            printf("Failure writing to destination file!\n");
            _aligned_free(out_file_buf);
            fclose(pInFile);
            fclose(pOutFile);
            lzham_dll.lzham_decompress_deinit(pComp_state);
            return FALSE;
         }
         
         total_output_bytes += out_num_bytes;
      }

      if ((status != LZHAM_COMP_STATUS_NOT_FINISHED) && (status != LZHAM_COMP_STATUS_NEEDS_MORE_INPUT))
         break;
   }
   
   src_bytes_left += (in_file_buf_size - in_file_buf_ofs);

   uint32 adler32 = lzham_dll.lzham_compress_deinit(pComp_state);
   pComp_state = NULL;
   
   uint64 end_time = GetTickCount64();
   double total_time = (max(1, end_time - start_time)) * 1.0f/1000.0f;
   
   _aligned_free(out_file_buf);
   out_file_buf = NULL;
   
   fclose(pInFile);
   pInFile = NULL;
   fclose(pOutFile);
   pOutFile = NULL;

   if (status != LZHAM_COMP_STATUS_SUCCESS)
   {
      printf("Compression failed with status %i\n", status);
      return FALSE;
   }

   if (src_bytes_left)
   {
      printf("Compressor failed to consume entire input file!\n");
      return FALSE;
   }

   printf("Success\n");
   printf("Input file size: %I64u, Compressed file size: %I64u, Ratio: %3.2f%%\n", src_file_size, total_output_bytes, (1.0f - (static_cast<float>(total_output_bytes) / src_file_size)) * 100.0f);
   printf("Compression time: %3.2f\nConsumption rate: %9.1f bytes/sec, Emission rate: %9.1f bytes/sec\n", total_time, src_file_size / total_time, total_output_bytes / total_time);
   printf("Input file adler32: 0x%08X\n", adler32);

   return TRUE;
}

static int decompress_file(lzham_dll_loader &lzham_dll, const char* pSrc_filename, const char *pDst_filename, bool unbuffered)
{
   printf("Testing: Streaming decompression\n");

   FILE *pInFile = NULL;
   FILE *pOutFile = NULL;

   fopen_s(&pInFile, pSrc_filename, "rb");
   if (!pInFile)
   {
      printf("Unable to read file: %s\n", pSrc_filename);
      return FALSE;
   }
   
   _fseeki64(pInFile, 0, SEEK_END);
   uint64 src_file_size = _ftelli64(pInFile);
   _fseeki64(pInFile, 0, SEEK_SET);
   if (src_file_size < 9)
   {
      printf("Compressed file is too small!\n");
      fclose(pInFile);
      return FALSE;
   }
   
   fopen_s(&pOutFile, pDst_filename, "wb");
   if (!pOutFile)
   {
      printf("Unable to create file: %s\n", pDst_filename);
      fclose(pInFile);
      return FALSE;
   }
   
   uint64 orig_file_size = 0;
   for (uint i = 0; i < 8; i++)
      orig_file_size |= (static_cast<uint64>(fgetc(pInFile)) << (i * 8));
   
   if ((unbuffered) && (orig_file_size > 1536*1024*1024))
   {
      printf("Output file is too large for unbuffered decompression - switching to streaming decompression.\n");
      unbuffered = false;
   }

   const uint cInBufSize = 65536*4;
   uint out_buf_size = unbuffered ? static_cast<uint>(orig_file_size) : 65536*4;
   
   uint8 in_file_buf[cInBufSize];
   uint8 *out_file_buf = static_cast<uint8*>(_aligned_malloc(out_buf_size, 16));
   if (!out_file_buf)
   {
      printf("Failed allocating output buffer!\n");
      fclose(pInFile);
      fclose(pOutFile);
      return FALSE;
   }

   uint64 src_bytes_left = src_file_size - sizeof(uint64);
   uint64 dst_bytes_left = orig_file_size;

   uint in_file_buf_size = 0;
   uint in_file_buf_ofs = 0;
      
   lzham_decompress_params params;
   memset(&params, 0, sizeof(params));
   params.m_struct_size = sizeof(lzham_decompress_params);
   params.m_dict_size_log2 = DICT_SIZE_LOG2;
   params.m_compute_adler32 = true;
   params.m_output_unbuffered = unbuffered;
   
   uint64 start_time = GetTickCount64();
      
   lzham_decompress_state_ptr pDecomp_state = lzham_dll.lzham_decompress_init(&params);
   if (!pDecomp_state)
   {
      printf("Failed initializing decompressor!\n");
      _aligned_free(out_file_buf);
      fclose(pInFile);
      fclose(pOutFile);
      return FALSE;
   }

   lzham_decompress_status_t status;
   for ( ; ; )
   {
      if (in_file_buf_ofs == in_file_buf_size)
      {
         in_file_buf_size = static_cast<uint>(min(cInBufSize, src_bytes_left));

         if (fread(in_file_buf, 1, in_file_buf_size, pInFile) != in_file_buf_size)
         {
            printf("Failure reading from source file!\n");
            _aligned_free(out_file_buf);
            lzham_dll.lzham_decompress_deinit(pDecomp_state);
            fclose(pInFile);
            fclose(pOutFile);
            return FALSE;
         }

         src_bytes_left -= in_file_buf_size;

         in_file_buf_ofs = 0;
      }

      uint8 *pIn_bytes = &in_file_buf[in_file_buf_ofs];
      size_t num_in_bytes = in_file_buf_size - in_file_buf_ofs;
      uint8* pOut_bytes = out_file_buf;
      size_t out_num_bytes = out_buf_size;

      status = lzham_dll.lzham_decompress(pDecomp_state, pIn_bytes, &num_in_bytes, pOut_bytes, &out_num_bytes, src_bytes_left == 0);

      if (num_in_bytes)
      {
         in_file_buf_ofs += (uint)num_in_bytes;
         assert(in_file_buf_ofs <= in_file_buf_size);
      }

      if (out_num_bytes)
      {
         if (fwrite(out_file_buf, 1, static_cast<uint>(out_num_bytes), pOutFile) != out_num_bytes)
         {
            printf("Failure writing to destination file!\n");
            _aligned_free(out_file_buf);
            lzham_dll.lzham_decompress_deinit(pDecomp_state);
            fclose(pInFile);
            fclose(pOutFile);
            return FALSE;
         }
        
         if (out_num_bytes > dst_bytes_left)
         {
            printf("Decompressor wrote too many bytes to destination file!\n");
            _aligned_free(out_file_buf);
            lzham_dll.lzham_decompress_deinit(pDecomp_state);
            fclose(pInFile);
            fclose(pOutFile);
            return FALSE;
         }
         dst_bytes_left -= out_num_bytes;
      }

      if ((status != LZHAM_DECOMP_STATUS_NOT_FINISHED) && (status != LZHAM_DECOMP_STATUS_NEEDS_MORE_INPUT))
         break;
   }

   _aligned_free(out_file_buf);
   out_file_buf = NULL;
   
   src_bytes_left += (in_file_buf_size - in_file_buf_ofs);

   uint32 adler32 = lzham_dll.lzham_decompress_deinit(pDecomp_state);
   pDecomp_state = NULL;
         
   uint64 end_time = GetTickCount64();
   double total_time = (max(1, end_time - start_time)) * 1.0f/1000.0f;
   
   fclose(pInFile); 
   pInFile = NULL;

   fclose(pOutFile);
   pOutFile = NULL;
   
   if (status != LZHAM_DECOMP_STATUS_SUCCESS)
   {
      printf("Decompression failed with status %i\n", status);
      return FALSE;
   }

   if (dst_bytes_left)
   {
      printf("Decompressor failed to output the entire output file!\n");
      return FALSE;
   }

   if (src_bytes_left)
   {
      printf("Decompressor failed to read %I64u bytes from input buffer\n", src_bytes_left);
   }

   printf("Success\n");
   printf("Compressed file size: %I64u, Decompressed file size: %I64u\n", src_file_size, orig_file_size);
   printf("Decompression time: %3.2f\nConsumption rate: %9.1f bytes/sec, Decompression rate: %9.1f bytes/sec\n", total_time, src_file_size / total_time, orig_file_size / total_time);
   printf("Decompressed adler32: 0x%08X\n", adler32);

   return TRUE;
}

static bool compare_files(const char *pFilename1, const char* pFilename2)
{
   FILE* pFile1 = NULL;
   fopen_s(&pFile1, pFilename1, "rb");
   if (!pFile1)
      return false;

   FILE* pFile2 = NULL;
   fopen_s(&pFile2, pFilename2, "rb");
   if (!pFile2)
   {
      fclose(pFile1);
      return false;
   }

   fseek(pFile1, 0, SEEK_END);
   int64 fileSize1 = _ftelli64(pFile1);
   fseek(pFile1, 0, SEEK_SET);

   fseek(pFile2, 0, SEEK_END);
   int64 fileSize2 = _ftelli64(pFile2);
   fseek(pFile2, 0, SEEK_SET);

   if (fileSize1 != fileSize2)
   {
      fclose(pFile1);
      fclose(pFile2);
      return false;
   }

   const uint cBufSize = 1024 * 1024;
   std::vector<uint8> buf1(cBufSize);
   std::vector<uint8> buf2(cBufSize);

   int64 bytes_remaining = fileSize1;
   while (bytes_remaining)
   {
      const uint bytes_to_read = static_cast<uint>(min(cBufSize, bytes_remaining));

      if (fread(&buf1.front(), bytes_to_read, 1, pFile1) != 1)
      {
         fclose(pFile1);
         fclose(pFile2);
         return false;
      }

      if (fread(&buf2.front(), bytes_to_read, 1, pFile2) != 1)
      {
         fclose(pFile1);
         fclose(pFile2);
         return false;
      }

      if (memcmp(&buf1.front(), &buf2.front(), bytes_to_read) != 0)
      {
         fclose(pFile1);
         fclose(pFile2);
         return false;
      }

      bytes_remaining -= bytes_to_read;
   }   

   fclose(pFile1);
   fclose(pFile2);
   return true;
}

typedef std::vector< std::string > string_array;

static bool find_files(std::string pathname, const std::string &filename, string_array &files, bool recursive)
{
   if (!pathname.empty())
   {
      char c = pathname[pathname.size() - 1];
      if ((c != ':') && (c != '\\') && (c != '/'))
         pathname += "\\";
   }
   
   WIN32_FIND_DATAA find_data;
   
   HANDLE findHandle = FindFirstFileA((pathname + filename).c_str(), &find_data);
   if (findHandle == INVALID_HANDLE_VALUE)
      return false;
   
   do
   {
      const bool is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      const bool is_system =  (find_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
      const bool is_hidden =  (find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;

      std::string filename(find_data.cFileName);

      if ((!is_directory) && (!is_system) && (!is_hidden))
         files.push_back(pathname + filename);

   } while (FindNextFileA(findHandle, &find_data));

   FindClose(findHandle);

   if (recursive)   
   {
      string_array paths;
      
      HANDLE findHandle = FindFirstFileA((pathname + "*").c_str(), &find_data);
      if (findHandle == INVALID_HANDLE_VALUE)
         return false;

      do
      {
         const bool is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
         const bool is_system =  (find_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
         const bool is_hidden =  (find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;

         std::string filename(find_data.cFileName);

         if ((is_directory) && (!is_hidden) && (!is_system))
            paths.push_back(filename);

      } while (FindNextFileA(findHandle, &find_data));

      FindClose(findHandle);
   
      for (uint i = 0; i < paths.size(); i++)
      {
         const std::string &path = paths[i];
         if (path[0] == '.')
            continue;

         if (!find_files(pathname + path, filename, files, true))
            return false;
      }
   }

   return true;
}

static bool ensure_file_is_writable(const char *pFilename)
{
   const int num_retries = 10;
   for (int i = 0; i < num_retries; i++)
   {
      FILE *pFile = NULL;
      fopen_s(&pFile, pFilename, "wb");
      if (pFile)
      {
         fclose(pFile);
         return true;
      }
      Sleep(500);
   }
   return false;
}

static int test_recursive(lzham_dll_loader &lzham_dll, const char *pPath, uint max_helper_threads, bool unbuffered_decompression)
{
   string_array files;
   if (!find_files(pPath, "*", files, true))
   {
      printf("Failed finding files under path \"%s\"!\n", pPath);
      return EXIT_FAILURE;
   }
   
   uint total_files_compressed = 0;
   uint64 total_source_size = 0;
   uint64 total_comp_size = 0;

   MEMORYSTATUS initial_mem_status;
   GlobalMemoryStatus(&initial_mem_status);

   uint start_tick_count = GetTickCount();

   const int first_file_index = 0;

   for (uint file_index = first_file_index; file_index < files.size(); file_index++)
   {
      const std::string &src_file = files[file_index];
      const std::string cmp_file("__comp__.bin");
      const std::string decomp_file("__decomp__.bin");

      printf("***** [%u of %u] Compressing file \"%s\" to \"%s\"\n", 1 + file_index, files.size(), src_file.c_str(), cmp_file.c_str());

      FILE *pFile = NULL;
      fopen_s(&pFile, src_file.c_str(), "rb");
      if (!pFile)
      {
         printf("Skipping unreadable file \"%s\"\n", src_file.c_str());
         continue;
      }
      fseek(pFile, 0, SEEK_END);
      int64 src_file_size = _ftelli64(pFile);
      fclose(pFile);
      
      if (unbuffered_decompression)
      {
         if ( src_file_size > 1300*1024*1024)
         {
            printf("Source file is too large for unbuffered decompression - skipping\n");
            continue;
         }
      }

      if (!ensure_file_is_writable(cmp_file.c_str()))
      {
         printf("Unable to create file \"%s\"!\n", cmp_file.c_str());
         return EXIT_FAILURE;
      }

      int status = compress_streaming(lzham_dll, src_file.c_str(), cmp_file.c_str(), max_helper_threads);
      if (!status)
      {
         printf("Failed compressing file \"%s\" to \"%s\"\n", src_file.c_str(), cmp_file.c_str());
         return EXIT_FAILURE;
      }

      printf("Decompressing file \"%s\" to \"%s\"\n", cmp_file.c_str(), decomp_file.c_str());

      if (!ensure_file_is_writable(decomp_file.c_str()))
      {
         printf("Unable to create file \"%s\"!\n", decomp_file.c_str());
         return EXIT_FAILURE;
      }
      
      status = decompress_file(lzham_dll, cmp_file.c_str(), decomp_file.c_str(), unbuffered_decompression);

      if (!status)
      {
         printf("Failed decompressing file \"%s\" to \"%s\"\n", src_file.c_str(), decomp_file.c_str());
         return EXIT_FAILURE;
      }

      printf("Comparing file \"%s\" to \"%s\"\n", decomp_file.c_str(), src_file.c_str());

      if (!compare_files(decomp_file.c_str(), src_file.c_str()))
      {
         printf("Failed comparing decompressed file data while compressing \"%s\" to \"%s\"\n", src_file.c_str(), cmp_file.c_str());
         return EXIT_FAILURE;
      }

      int64 cmp_file_size = 0;
      fopen_s(&pFile, cmp_file.c_str(), "rb");
      if (pFile)
      {
         fseek(pFile, 0, SEEK_END);
         cmp_file_size = _ftelli64(pFile);
         fclose(pFile);
      }
      
      total_files_compressed++;
      total_source_size += src_file_size;
      total_comp_size += cmp_file_size;

      MEMORYSTATUS mem_status;
      GlobalMemoryStatus(&mem_status);

#ifdef _XBOX      
      const int64 bytes_allocated = initial_mem_status.dwAvailPhys - mem_status.dwAvailPhys;
#else
      const int64 bytes_allocated = initial_mem_status.dwAvailVirtual- mem_status.dwAvailVirtual;
#endif      

      printf("Memory allocated relative to first file: %I64i\n", bytes_allocated);
      printf("\n");

      //anvil_malloc_dump_stats();
   }
   
   uint end_tick_count = GetTickCount();

   double total_elapsed_time = (end_tick_count - start_tick_count) / 1000.0f;

   printf("Test successful: %f secs\n", total_elapsed_time);
   printf("Total files processed: %u\n", total_files_compressed);
   printf("Total source size: %I64u\n", total_source_size);
   printf("Total compressed size: %I64u\n", total_comp_size);

   return EXIT_SUCCESS;
}

static void print_usage()
{
   printf("usage: [a/A/c/D/d] inpath/infile [outfile]\n");
   printf("a - Compress all files under \"inpath\"\n");
   printf("A - Compress all files under \"inpath\" (unbuffered decompression)\n");
   printf("c - Compress \"infile\" to \"outfile\"\n");
   printf("d - Decompress \"infile\" to \"outfile\"\n");
   printf("u - Decompress \"infile\" to \"outfile\" (unbuffered decompression)\n");
}

int main(int argc, char *argv[])
{
   lzham_dll_loader lzham_dll;

   wchar_t lzham_dll_filename[MAX_PATH];
   lzham_dll_loader::create_module_path(lzham_dll_filename, MAX_PATH, g_is_debug);

   printf("Dynamically loading DLL \"%S\"\n", lzham_dll_filename);

   HRESULT hres = lzham_dll.load(lzham_dll_filename);
   if (FAILED(hres))
   {
      printf("Failed loading LZHAM DLL (Status=0x%04X)!\n", hres);
      return EXIT_FAILURE;
   }
   
   printf("Loaded LZHAM DLL version 0x%04X\n", lzham_dll.lzham_get_version());

   if (argc == 1)
   {
      print_usage();
   
      return simple_test(lzham_dll);
   }
   
   SYSTEM_INFO g_system_info;
   GetSystemInfo(&g_system_info);  
   int num_helper_threads = 0;
   if (g_system_info.dwNumberOfProcessors > 1)
   {
      num_helper_threads = g_system_info.dwNumberOfProcessors - 1;
   }
   
   if ((argc == 3) && (tolower(argv[1][0]) == 'a'))
   {
      bool unbuffered_decompression = (argv[1][0] == 'A');
      return test_recursive(lzham_dll, argv[2], num_helper_threads, unbuffered_decompression);
   }
   
   if (argc != 4)
   {
      print_usage();
      return EXIT_FAILURE;
   }

   int status = EXIT_SUCCESS;
   
   if (argv[1][0] == 'c')
   {
      if (!compress_streaming(lzham_dll, argv[2], argv[3], num_helper_threads))
         status = EXIT_FAILURE;
   }
   else if (argv[1][0] == 'd')
   {
      if (!decompress_file(lzham_dll, argv[2], argv[3], false))
         status = EXIT_FAILURE;
   }
   else if (argv[1][0] == 'D')
   {
      if (!decompress_file(lzham_dll, argv[2], argv[3], true))
         status = EXIT_FAILURE;
   }
   else
   {
      printf("Invalid mode: %s\n", argv[1]);
      status = EXIT_FAILURE;
   }
   
   lzham_dll.unload();
   
   return status;
}

