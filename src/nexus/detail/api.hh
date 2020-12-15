#pragma once

#include <clean-core/macros.hh>

#ifdef CC_OS_WINDOWS

#ifdef NX_BUILD_DLL

#ifdef NX_DLL
#define NX_API __declspec(dllexport)
#else
#define NX_API __declspec(dllimport)
#endif

#else
#define NX_API
#endif

#else
#define NX_API
#endif
