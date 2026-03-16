
#pragma once

#include "../platform/PlatformConfig.h"

#include <cassert>

#pragma warning(disable: 4251)
#pragma warning(disable: 4275)

#define NS_JYE_BEGIN                     
#define NS_JYE_END                       
#define USING_JYE_CC                     

NS_JYE_BEGIN

#ifdef PLATFORM_WIN32
	#if defined(STATIC)
		#define DLL 
	#else
		#if defined(_USRDLL)
			#define DLL     __declspec(dllexport)
		#else         /* use a DLL library */
			#define DLL     __declspec(dllimport)
		#endif
	#endif
#else
	#define DLL 
#endif

#ifdef PLATFORM_WIN32
#include <BaseTsd.h>
#ifndef __SSIZE_T
	#define __SSIZE_T
	typedef SSIZE_T ssize_t;
#endif // __SSIZE_T
#endif // PLATFORM_WIN32

// FIXME:: Backward compatible
#define Assert ASSERT

#define JY_ASSERT(cond)    assert(cond)

#ifdef JYE_DEBUG
	#define ASSERT(f) assert(f)
	#define DEBUGCODE(x) x
#else
	#define ASSERT(f)
	#define DEBUGCODE(x)
#endif

#ifdef PLATFORM_WIN32
#define FORCEINLINE __forceinline
#else
#define FORCEINLINE __attribute__((always_inline)) inline
#endif

#define SAFE_DELETE(p) if(p){delete p; p = NULL;}
#define SAFE_DELETE_ARRAY(p) if(p){delete[] p; p = NULL;}

#ifndef NOMINMAX	// max and std::max conflict
   #define NOMINMAX
#endif


typedef unsigned char		byte;
typedef unsigned short		ushort;
typedef unsigned short		uint16;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef unsigned char		byte;
typedef unsigned int		bitmask;
typedef int					sint;
typedef long long			int64;
typedef unsigned long long	uint64;
typedef unsigned int		handle;

#define nullhandle ((uint)-1)
#define nullid ((uint64)-1)

NS_JYE_END