// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>



// TODO: reference additional headers your program requires here
#include <tchar.h>

#ifdef _DEBUG
  #define ASSERT(x) { if(!(x)) DebugBreak(); }
  #define VERIFY(x)  { if(!(x)) DebugBreak(); }
  #define UNUSED(x)
  #define UNUSED_ALWAYS(x) (x)
#else
  #define ASSERT(x)
  #define VERIFY(x) (x)
  #define UNUSED(x) (x)
  #define UNUSED_ALWAYS(x) (x)
#endif
