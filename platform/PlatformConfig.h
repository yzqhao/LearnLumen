
#pragma once

/// @cond DO_NOT_SHOW

//////////////////////////////////////////////////////////////////////////
// pre configure
//////////////////////////////////////////////////////////////////////////

// define supported target platform macro which CC uses.
#define PLATFORM_UNKNOWN          

// Apple: Mac and iOS
#if __APPLE__ && !ANDROID // exclude android for binding generator.
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE // TARGET_OS_IPHONE includes TARGET_OS_IOS TARGET_OS_TV and TARGET_OS_WATCH. see TargetConditionals.h
        #define         PLATFORM_IOS 1
    #elif TARGET_OS_MAC
        #define         PLATFORM_MAC 1
    #endif
#endif

// android
#if ANDROID
    #define         PLATFORM_ANDROID 1
#endif

// win32
#if _WIN32 || _WINDOWS
    #define         PLATFORM_WIN32 1
#endif

//////////////////////////////////////////////////////////////////////////
// post configure
//////////////////////////////////////////////////////////////////////////

#if PLATFORM_WIN32
#ifndef __MINGW32__
#pragma warning (disable:4127)
#endif
#endif  // PLATFORM_WIN32

#if (PLATFORM_ANDROID) || (PLATFORM_IOS) || (PLATFORM_TIZEN)
    #define PLATFORM_MOBILE
#else
    #define PLATFORM_PC
#endif

#if (PLATFORM_ANDROID) || (PLATFORM_IOS) || (PLATFORM_MAC)
    #define PLATFORM_UNIX
#endif

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <list>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#if PLATFORM_WIN32
#include <dwmapi.h>
#include <ShlObj.h>
#include <WinNls.h>
#include <wchar.h>
#include <tchar.h>
#include <io.h>
#include <direct.h>
#include <ShObjIdl.h>
#include <shellapi.h>
#include <ObjBase.h>
#define NOMINMAX
#include <time.h>
#ifndef TCHAR
#define WCHAR TCHAR
#endif
#elif (PLATFORM_ANDROID) || (PLATFORM_IOS) || (PLATFORM_MAC)
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <net/if.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

#if (PLATFORM_IOS)
#include <ifaddrs.h>
#endif

#if (PLATFORM_ANDROID)
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#define  LOG_TAG    "JYE"
#define JAVA_API(return_type) extern "C" JNIEXPORT return_type JNICALL
#define JFOO(CLS,METHOD) Java_cn_battlefire_androidplayer_##CLS##_##METHOD
#endif
typedef int64_t __int64;
#endif


