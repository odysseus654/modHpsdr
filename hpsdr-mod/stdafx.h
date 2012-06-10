// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define _CRTDBG_MAP_ALLOC

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

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

#define PURE =0