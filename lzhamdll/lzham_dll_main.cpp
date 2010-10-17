// File: lzham_dll_main.cpp
#include "lzham_core.h"

BOOL APIENTRY DllMain(HANDLE hModule, DWORD fdwReason, LPVOID lpReserved)
{
   hModule, fdwReason, lpReserved;

   switch( fdwReason ) 
   { 
      case DLL_PROCESS_ATTACH:
      {
         break;
      }
      case DLL_PROCESS_DETACH:
      {
         break;
      }
   }

   return TRUE;
}

